#include <dpu.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include "common.h"

#ifndef DPU_BINARY
#define DPU_BINARY "build/dpu"
#endif

// Structure to define a point
typedef struct Point
{
    float x, y;
} Point;

// Structure to define a bounding box (MBR)
typedef struct MBR
{
    float xmin, ymin;
    float xmax, ymax;
} MBR;

// Structure for a node
typedef struct Node
{
    int isLeaf; // 1 if it's a leaf node, 0 if it's an internal node
    int count;  // Number of entries in the node
    MBR mbr;    // Bounding box for the node
    union
    {
        struct Node **children; // Child nodes (internal node)
        Point *points;          // Points (leaf node)
    };
} Node;

// Structure for a serialized node
typedef struct SerializedNode
{
    int isLeaf;       // 1 if it's a leaf node, 0 if it's an internal node
    int count;        // Number of entries in the node
    MBR mbr;          // Bounding box for the node
    int children[16]; // Indices of child nodes (internal node), max FANOUT assumed to be
    Point points[16]; // Points (leaf node), max BUNDLEFACTOR assumed to be
} SerializedNode;

// Forward declarations for helper functions
int readPointsFromFile(const char *filename, Point points[], int max_points);
void printPoints(Point points[], int num_points);
Node *createRTree(Point *ptArr, int low, int high);
void printRTree(Node *node, int level);
bool searchRTree(Node *node, Point queryPoint);
void Zsorting(Point points[], int num_points);
int serialize_rtree_wrapper(Node *root, SerializedNode **output, int max_nodes);
void print_serialisedtree(int node_index, int depth, SerializedNode *serialized_tree);
Node *getSubtree(Node *root, int targetIndex);
// int countNodesInSubtree(Node *root);
SerializedNode *serialized_tree;

int main()
{
    struct dpu_set_t dpu_set, dpu;
    uint32_t nr_of_dpus;
    bool status = true;
    bool result_host = false;
    // uint64_t dpu_index = 0;

    clock_t start_time, end_time;
    double rtree_construction_time;

    Point points[MAX_POINTS];
    // int numPoints = readPointsFromFile("/home/tjv7w/PIM/RtreeCPU_SingleQuery/Data/datapoint.txt", points, MAX_POINTS);
    //int numPoints = readPointsFromFile("/home/tjv7w/PIM/RtreeCPU_QueryM/Data/gaussian_data_points_100k.csv", points, MAX_POINTS);
    int numPoints = readPointsFromFile("gaussian_data_points_1M.csv", points, MAX_POINTS);
    if (numPoints <= 0)
    {
        printf("Failed to read points from the file.\n");
        return 1;
    }
    Zsorting(points, numPoints);
    // printf("\nSorted Points by Z-value:\n");
    // printPoints(points, numPoints);

    // Build the R-tree on the host
    start_time = clock();
    Node *root = createRTree(points, 0, numPoints - 1);
    end_time = clock();
    rtree_construction_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    printf("R-tree construction time: %.3f μs\n", rtree_construction_time * 1000000);
    // printRTree(root, 0);
    Point query_point = {4792855.00, 6027188.00};
    // Point query_point = {4992113,5435896};
    start_time = clock();

    result_host = searchRTree(root, query_point);
    if (result_host)
    {
        printf("\nQuery point (%.1f, %.1f) " ANSI_COLOR_GREEN "FOUND" ANSI_COLOR_RESET " in R-tree in HOST", query_point.x, query_point.y);
    }
    end_time = clock();
    double search_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;

    printf(ANSI_COLOR_LIGHT_BLUE "\nTime taken to search the point in HOST is %.3f μs" ANSI_COLOR_RESET "\n\n", search_time * 1000000);

    // Preparing to send DPU

    // Allocate DPU set and load the DPU program
    DPU_ASSERT(dpu_alloc(NR_DPUS, NULL, &dpu_set));
    DPU_ASSERT(dpu_load(dpu_set, DPU_BINARY, NULL));

    DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &nr_of_dpus));
    printf("\nAllocated %d DPU(s)\n", nr_of_dpus);

    printf("\nPassing Tree and Query to DPUs...\n");

    printf("\nPassing Data Points DPUs...\n");

    uint64_t dpu_id = 0;
    // int currentIndex=0;
    int start_index = 1;
    DPU_FOREACH(dpu_set, dpu)
    {
        Node *subtree = getSubtree(root, start_index);
        // printf("\n\n Subtree in %lu\n", dpu_id);
        // printRTree(subtree, 0);
        int num_nodes = serialize_rtree_wrapper(subtree, &serialized_tree, MAX_NODES);
        start_index += num_nodes;
        // print_serialisedtree(0, 0, serialized_tree);

        //printf("\n %d nodes send to DPU id =%lu\n", num_nodes, dpu_id);
        DPU_ASSERT(dpu_copy_to(dpu, "DPU_INDEX", 0, &dpu_id, sizeof(uint64_t)));
        DPU_ASSERT(dpu_copy_to(dpu, "DPU_TREE", 0, serialized_tree, num_nodes * sizeof(SerializedNode)));
        dpu_id++;
    }

    printf("\nSend Query value to DPUs...\n");

    DPU_FOREACH(dpu_set, dpu)
    {
        DPU_ASSERT(dpu_copy_to(dpu, "DPU_QUERY_POINT", 0, &query_point, sizeof(Point)));
    }

    printf("\nRunning Query on DPU(s)...\n");

    clock_t dpu_kernel_start_time = clock();
    DPU_ASSERT(dpu_launch(dpu_set, DPU_SYNCHRONOUS));

    // Capture time after DPU search is done
    clock_t dpu_kernel_end_time = clock();
    double dpu_kernel_time = ((double)(dpu_kernel_end_time - dpu_kernel_start_time)) / CLOCKS_PER_SEC;

    DPU_FOREACH(dpu_set, dpu)
    {
        DPU_ASSERT(dpu_log_read(dpu, stdout));
    }
    printf(ANSI_COLOR_LIGHT_BLUE "\nDPU Kernel Time %.3f μs" ANSI_COLOR_RESET "\n\n", dpu_kernel_time * 1000000);

    // Retrieve results from DPUs after execution
    printf("\nRetrieve results from DPUs after execution\n");
    dpu_id = 0;
    uint64_t dpu_result;
    clock_t DPU_CPU_start_time = clock();
    DPU_FOREACH(dpu_set, dpu)
    {

        DPU_ASSERT(dpu_copy_from(dpu, "DPU_RESULT", 0, &dpu_result, sizeof(uint64_t)));
        // printf("\ndpu_id=%lu, dpu_result= %lu",dpu_id,dpu_result);

        if (dpu_result)
        {
            printf("\nQuery point (%.1f, %.1f) " ANSI_COLOR_GREEN "FOUND" ANSI_COLOR_RESET " in R-tree in DPU: %lu.\n", query_point.x, query_point.y, dpu_id);
        }
        else
        {
            //  end_time = clock();
            // search_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;

            //  printf("\n Time taken to search the point in DPU is %.3f μs\n", search_time * 1000000);

            // printf("\nQuery point (%.1f, %.1f) " ANSI_COLOR_RED "NOT FOUND" ANSI_COLOR_RESET " in R-tree in DPU: %lu.\n", query_point.x, query_point.y, dpu_id);
        }
        dpu_id++;
    }
    clock_t DPU_CPU_end_time = clock();
    double DPU_CPU_search_time = ((double)(DPU_CPU_end_time - DPU_CPU_start_time)) / CLOCKS_PER_SEC;

    printf(ANSI_COLOR_LIGHT_BLUE "\nDPU-CPU Time %.3f μs " ANSI_COLOR_RESET "\n\n", DPU_CPU_search_time * 1000000);

    // Free the DPU set
    DPU_ASSERT(dpu_free(dpu_set));

    return status ? 0 : -1;
}

#include <defs.h>
#include <mram.h>
#include <perfcounter.h>
#include <stdint.h>
#include <stdio.h>

#include "common.h"

#define BLOCK_SIZE (256)

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

// Structure for a serialized node
typedef struct SerializedNode
{
    int isLeaf;       // 1 if it's a leaf node, 0 if it's an internal node
    int count;        // Number of entries in the node
    MBR mbr;          // Bounding box for the node
    int children[16]; // Indices of child nodes (internal node), max FANOUT assumed to be 16
    Point points[16]; // Points (leaf node), max BUNDLEFACTOR assumed to be 16
} SerializedNode;

// MRAM Variables
__mram_noinit uint64_t DPU_INDEX;
__mram_noinit Point DPU_QUERY_POINT;
__mram_noinit SerializedNode DPU_TREE[MAX_NODES];
__mram_noinit uint64_t DPU_RESULT;

// Recursive function to search a query point in the serialized R-tree
bool search_rtree_dpu(int node_index, Point query_point, int start_child, int end_child)
{
    // Check if the query point is within the MBR of the current node
    if (!(query_point.x >= DPU_TREE[node_index].mbr.xmin && query_point.x <= DPU_TREE[node_index].mbr.xmax && query_point.y >= DPU_TREE[node_index].mbr.ymin && query_point.y <= DPU_TREE[node_index].mbr.ymax))
    {
        return false; // Query point is outside the MBR
    }

    if (DPU_TREE[node_index].isLeaf)
    {
        // Search in the points of the leaf node
        for (int i = 0; i < DPU_TREE[node_index].count; i++)
        {
            if (query_point.x == DPU_TREE[node_index].points[i].x && query_point.y == DPU_TREE[node_index].points[i].y)
            {
                return true;
            }
        }
        return false; // Not found in this leaf node
    }
    else
    {
        // Recursively search in the assigned child nodes
        for (int i = start_child; i < end_child; i++)
        {
            if (i >= DPU_TREE[node_index].count) // Ensure we don't access out-of-bounds
                break;

            if (search_rtree_dpu(DPU_TREE[node_index].children[i], query_point, 0, DPU_TREE[DPU_TREE[node_index].children[i]].count))
            {
                return true;
            }
        }
        return false; // Not found in any child nodes
    }
}

int main()
{
    DPU_RESULT = 0;
    uint32_t tasklet_id = me(); // Tasklet ID (0 to NR_TASKLETS - 1)
    uint64_t dpu_index = DPU_INDEX;

    if (tasklet_id == 0)
    {
       // printf("\n|| Printing DPU_TREE in DPU ID: %lu ||\n", dpu_index);
    }

    // Calculate the number of children per tasklet
    int total_children = DPU_TREE[0].count; // Root node's child count
    int children_per_tasklet = total_children / NR_TASKLETS;
    uint32_t extra_children = total_children % NR_TASKLETS;

    // Assign range of children to each tasklet
    int start_child = tasklet_id * children_per_tasklet + (tasklet_id < extra_children ? tasklet_id : extra_children);
    int end_child = start_child + children_per_tasklet + (tasklet_id < extra_children ? 1 : 0);

    // Perform the search on the assigned range of children
    bool found = search_rtree_dpu(0, DPU_QUERY_POINT, start_child, end_child);

    
    if (found)
    {
        DPU_RESULT = 1;
        printf("\nTasklet %d " ANSI_COLOR_GREEN "found" ANSI_COLOR_RESET " query point (%.1f, %.1f) on DPU ID: %lu.\n",
               tasklet_id, DPU_QUERY_POINT.x, DPU_QUERY_POINT.y, dpu_index);
    }

    

    return 0;
}

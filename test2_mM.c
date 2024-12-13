#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <stdbool.h>
#include<string.h>
/*
#define BUNDLEFACTOR 5 // Number of points to form a leaf node
#define FANOUT 4       // Number of children per non-leaf node
*/

#define M 7
#define m 3


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
    int isLeaf;          // 1 if it's a leaf node, 0 if it's an internal node
    int count;           // Number of entries in the node
    MBR mbr;             // Bounding box for the node
    int children[16];    // Indices of child nodes (internal node), max FANOUT assumed to be 16
    Point points[16];    // Points (leaf node), max BUNDLEFACTOR assumed to be 16
} SerializedNode;

// Function to initialize a bounding box
void initMBR(MBR *mbr)
{
    mbr->xmin = FLT_MAX;
    mbr->ymin = FLT_MAX;
    mbr->xmax = -FLT_MAX;
    mbr->ymax = -FLT_MAX;
}

// Function to update the MBR with a point
void updateMBRWithPoint(MBR *mbr, Point p)
{
    if (p.x < mbr->xmin)
        mbr->xmin = p.x;
    if (p.y < mbr->ymin)
        mbr->ymin = p.y;
    if (p.x > mbr->xmax)
        mbr->xmax = p.x;
    if (p.y > mbr->ymax)
        mbr->ymax = p.y;
}

// Function to compute the union of two MBRs
MBR *unionJoin(MBR *mbr1, MBR *mbr2)
{
    MBR *result = (MBR *)malloc(sizeof(MBR));
    result->xmin = (mbr1->xmin < mbr2->xmin) ? mbr1->xmin : mbr2->xmin;
    result->ymin = (mbr1->ymin < mbr2->ymin) ? mbr1->ymin : mbr2->ymin;
    result->xmax = (mbr1->xmax > mbr2->xmax) ? mbr1->xmax : mbr2->xmax;
    result->ymax = (mbr1->ymax > mbr2->ymax) ? mbr1->ymax : mbr2->ymax;
    return result;
}

// Function to create a leaf node
Node *createLeaf(Point *ptArr, int low, int high)
{
    Node *leaf = (Node *)malloc(sizeof(Node));
    leaf->isLeaf = 1;
    leaf->count = high - low + 1;
    leaf->points = (Point *)malloc(leaf->count * sizeof(Point));

    // Copy points into the leaf node and compute its MBR
    initMBR(&leaf->mbr);
    for (int i = low; i <= high; i++)
    {
        leaf->points[i - low] = ptArr[i];
        updateMBRWithPoint(&leaf->mbr, ptArr[i]);
    }
    return leaf;
}

/*
// Function to get the range of points for each child node
void getRange(int *range, int childID, int low, int high)
{
    int rangeSize = high - low + 1;
    int partitionSize = rangeSize / FANOUT;

    range[0] = low + childID * partitionSize;
    if (childID == FANOUT - 1)
    {
        range[1] = high;
    }
    else
    {
        range[1] = range[0] + partitionSize - 1;
    }
}
/*

// Function to create an R-tree node recursively
Node *createRTree(Point *ptArr, int low, int high)
{
    Node *root;
    printf("\n\nValue of low=%d and high=%d and diff=%d",low,high,(high - low + 1));

    if ((high - low + 1) <= BUNDLEFACTOR)
    {
        // If the number of points is less than or equal to the bundle factor, create a leaf node
        root = createLeaf(ptArr, low, high);
        
    }
    else
    {
        // Otherwise, create an internal node
        root = (Node *)malloc(sizeof(Node));
        root->isLeaf = 0;
        root->count = FANOUT;

        // Temporary array to hold children nodes
        Node **tempChildren = (Node **)malloc(FANOUT * sizeof(Node *));

        for (int childID = 0; childID < FANOUT; childID++)
        {
            int range[2];
            getRange(range, childID, low, high);
            printf("\nrange of non leaf %d-%d ",range[0], range[1]);

            Node *child = createRTree(ptArr, range[0], range[1]);
            tempChildren[childID] = child;
        }

        root->children = tempChildren;

        // Calculate the MBR for the internal node
        MBR *parentMBR = (MBR *)malloc(sizeof(MBR));
        initMBR(parentMBR);
        for (int i = 0; i < FANOUT; i++)
        {
            *parentMBR = *unionJoin(parentMBR, &root->children[i]->mbr);
        }
        root->mbr = *parentMBR;
        printf("\nCreating rtree mbr (%.1f,%.1f), (%.1f,%.1f) ",root->mbr.xmin,root->mbr.ymin,root->mbr.xmax,root->mbr.ymax );
    }

    return root;
}
*/


Node *createRTree(Point *ptArr, int low, int high)
{
    Node *root;
    printf("\n\nValue of low=%d and high=%d and diff=%d",low,high,(high - low + 1));

    int numPoints = high - low + 1;

    // If the number of points is less than or equal to M, create a leaf node
    if (numPoints <= M)
    {
        root = createLeaf(ptArr, low, high);
    }
    else
    {
        // Otherwise, create an internal node
        root = (Node *)malloc(sizeof(Node));
        root->isLeaf = 0;

        // Determine the number of child nodes based on m and M constraints
        int numChildren = (numPoints + M - 1) / M; // Ceil(numPoints / M)
        numChildren = (numChildren < m) ? m : numChildren; // Ensure at least m children

        root->count = numChildren;

        // Temporary array to hold children nodes
        Node **tempChildren = (Node **)malloc(numChildren * sizeof(Node *));

        // Divide points among children while maintaining constraints
        int rangeSize = numPoints / numChildren;
        int remaining = numPoints % numChildren;

        int start = low;
        for (int childID = 0; childID < numChildren; childID++)
        {
            int end = start + rangeSize - 1;
            if (remaining > 0)
            {
                end++;
                remaining--;
            }

            printf("\nrange of non-leaf child %d-%d ", start, end);

            Node *child = createRTree(ptArr, start, end);
            tempChildren[childID] = child;

            start = end + 1;
        }

        root->children = tempChildren;

        // Calculate the MBR for the internal node
        MBR *parentMBR = (MBR *)malloc(sizeof(MBR));
        initMBR(parentMBR);
        for (int i = 0; i < numChildren; i++)
        {
            *parentMBR = *unionJoin(parentMBR, &root->children[i]->mbr);
        }
        root->mbr = *parentMBR;

        printf("\nCreating R-tree MBR (%.1f, %.1f), (%.1f, %.1f) ", 
               root->mbr.xmin, root->mbr.ymin, root->mbr.xmax, root->mbr.ymax);
    }

    return root;
}

// Function to print the R-tree (for debugging)
void printRTree(Node *node, int level)
{
    for (int i = 0; i < level; i++)
        printf("  ");

    printf("Node (isLeaf=%d, count=%d, MBR=[%.2f, %.2f, %.2f, %.2f])\n",
           node->isLeaf, node->count, node->mbr.xmin, node->mbr.ymin, node->mbr.xmax, node->mbr.ymax);

    if (node->isLeaf)
    {
        for (int i = 0; i < node->count; i++)
        {
            for (int j = 0; j <= level; j++)
                printf("  ");
            printf("Point (%.2f, %.2f)\n", node->points[i].x, node->points[i].y);
        }
    }
    else
    {
        for (int i = 0; i < node->count; i++)
        {
            printRTree(node->children[i], level + 1);
        }
    }
}

// Function to check if a point is within an MBR
bool isPointInMBR(MBR *mbr, Point p)
{
    return (p.x >= mbr->xmin && p.x <= mbr->xmax &&
            p.y >= mbr->ymin && p.y <= mbr->ymax);
}

// Function to search for a point in the R-tree
bool searchRTree(Node *node, Point queryPoint)
{
    // Check if the point lies within the current node's MBR
    if (!isPointInMBR(&node->mbr, queryPoint))
    {
        return false; // Point is outside this node's MBR
    }

    if (node->isLeaf)
    {
        // If it's a leaf node, search for the point in the points array
        for (int i = 0; i < node->count; i++)
        {
            if (node->points[i].x == queryPoint.x && node->points[i].y == queryPoint.y)
            {
                return true; // Point found
            }
        }
        return false; // Point not found in this leaf
    }
    else
    {
        // If it's an internal node, recursively search the children
        for (int i = 0; i < node->count; i++)
        {
            if (searchRTree(node->children[i], queryPoint))
            {
                printf("\nSearching in mbr (%.1f,%.1f), (%.1f,%.1f) ",node->mbr.xmin,node->mbr.ymin,node->mbr.xmax,node->mbr.ymax );
                return true; // Point found in one of the children
            }
        }
        return false; // Point not found in any children
    }
}

// Function to read points from a file
int readPointsFromFile(const char *filename, Point points[], int max_points) {
    int num_points = 0;

    // Open the file for reading
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Unable to open file");
        return -1;
    }

    // Get file size and allocate buffer
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = (char *)malloc(file_size + 1);
    if (!buffer) {
        perror("Unable to allocate memory");
        fclose(file);
        return -1;
    }

    // Read file contents into buffer and null-terminate
    fread(buffer, 1, file_size, file);
    buffer[file_size] = '\0';  // Null-terminate to safely use as a string

    // Close the file after reading
    fclose(file);

    // Parse points from the buffer
    char *line = buffer;
    while (num_points < max_points && sscanf(line, "%f, %f", &points[num_points].x, &points[num_points].y) == 2) {
        num_points++;
        // Move to the next line or point
        line = strchr(line, '\n');
        if (!line) break;  // Exit if no more lines
        line++;  // Move to the start of the next line
    }

    // Free the buffer after parsing
    free(buffer);

    return num_points; // Return the number of points read
}
  
void printPoints(Point points[], int num_points) {
    for (int i = 0; i < num_points; i++) {
        printf("(%.1f, %.1f) ", points[i].x, points[i].y);
    }
    printf("\n");
}



// Recursive function to serialize the R-tree
int serialize_rtree(Node *node, SerializedNode *serialized_tree, int *current_index)
{
    if (node == NULL)
        return -1; // Return an invalid index if the node is NULL

    // Get the index for this node in the serialized array
    int index = (*current_index)++;

    // Populate the serialized node
    serialized_tree[index].isLeaf = node->isLeaf;
    serialized_tree[index].count = node->count;
    serialized_tree[index].mbr = node->mbr;

    if (node->isLeaf)
    {
        // Copy points for a leaf node
        memcpy(serialized_tree[index].points, node->points, node->count * sizeof(Point));
    }
    else
    {
        // Recursively serialize child nodes for an internal node
        for (int i = 0; i < node->count; i++)
        {
            serialized_tree[index].children[i] = serialize_rtree(node->children[i], serialized_tree, current_index);
        }
    }

    return index; // Return the index of the current node
}

// Wrapper function to start serialization
int serialize_rtree_wrapper(Node *root, SerializedNode **output, int max_nodes)
{
    // Allocate memory for the serialized tree
    *output = (SerializedNode *)malloc(max_nodes * sizeof(SerializedNode));
    if (*output == NULL)
    {
        perror("Failed to allocate memory for serialized tree");
        return -1;
    }

    int current_index = 0;
    serialize_rtree(root, *output, &current_index);
    return current_index; // Return the total number of serialized nodes
}
bool searchSerializedTree(SerializedNode *serialized_tree, int current_index, Point queryPoint)
{
    if (current_index < 0)
        return false;

    SerializedNode *current_node = &serialized_tree[current_index];

    // Check if the point lies within the current node's MBR
    if (!isPointInMBR(&current_node->mbr, queryPoint))
    {
        return false; // Point is outside this node's MBR
    }

    if (current_node->isLeaf)
    {
        // If it's a leaf node, search for the point in the points array
        for (int i = 0; i < current_node->count; i++)
        {
            if (current_node->points[i].x == queryPoint.x && current_node->points[i].y == queryPoint.y)
            {
                return true; // Point found
            }
        }
        return false; // Point not found in this leaf
    }
    else
    {
        // If it's an internal node, recursively search the children
        for (int i = 0; i < current_node->count; i++)
        {
            if (searchSerializedTree(serialized_tree, current_node->children[i], queryPoint))
            {
                return true; // Point found in one of the children
            }
        }
        return false; // Point not found in any children
    }
}


int main()
{
    // Your provided set of points
  //  Point points[] = {{30, 31}, {26, 48}, {31, 50}, {43, 38}, {38, 44}, {44, 41}, {44, 42}, {44, 43}, {45, 43}, {43, 45}, {47, 47}, {49, 36}, {50, 37}, {55, 38}, {53, 41}, {36, 54}, {42, 51}, {44, 50}, {45, 50}, {40, 53}, {42, 52}, {43, 52}, {46, 52}, {33, 57}, {35, 56}, {32, 60}, {34, 62}, {37, 61}, {47, 58}, {45, 48}, {53, 50}, {49, 54}, {48, 55}, {51, 52}, {53, 52}, {54, 52}, {59, 49}, {61, 48}, {57, 52}, {52, 57}, {49, 61}, {51, 61}, {76, 40}, {72, 48}, {75, 53}, {67, 62}, {46, 68}, {49, 67}, {50, 66}, {58, 76}};
    
    Point points[256]; // Adjust the size based on the expected number of points
    // Open the file for reading
    //int numPoints = readPointsFromFile("gaussian_data_points_1M.csv", points, 256);
    int numPoints = readPointsFromFile("/home/tjv7w/PIM/RtreeCPU_QueryM/Data/datapoint.txt", points, 256);
     printf("\nOriginal Points:\n");
   // printPoints(points, numPoints);

    
    // Create the R-tree
    Node *root = createRTree(points, 0, numPoints - 1);

    // Print the R-tree structure
    printf("\nRtree \n");
    printRTree(root, 0);
    // Serialize the R-tree
    SerializedNode *serialized_tree;
    int num_nodes = serialize_rtree_wrapper(root, &serialized_tree, 100);
    if (num_nodes <= 0) {
        printf("Failed to serialize the R-tree.\n");
        return 1;
    }
    printf("\nSerialized R-tree with %d nodes.\n", num_nodes);
    // Output the serialized tree for debugging
    
    for (int i = 0; i < num_nodes; i++)
    {
        printf("Node %d: isLeaf=%d, count=%d, MBR=[%.1f, %.1f, %.1f, %.1f]\n",
               i, serialized_tree[i].isLeaf, serialized_tree[i].count,
               serialized_tree[i].mbr.xmin, serialized_tree[i].mbr.ymin,
               serialized_tree[i].mbr.xmax, serialized_tree[i].mbr.ymax);
    }

    // Free allocated memory
    free(serialized_tree);

    // Query point
    Point queryPoint = {44, 42};

    // Search for the point
    if (searchRTree(root, queryPoint))
    {
        printf("\nPoint (%f, %f) found in the R-tree.\n", queryPoint.x, queryPoint.y);
    }
    else
    {
        printf("\nPoint (%f, %f) not found in the R-tree.\n", queryPoint.x, queryPoint.y);
    }


    // Search for the point in the serialized tree
    if (searchSerializedTree(serialized_tree, 0, queryPoint))
    {
        printf("\nPoint (%f, %f) found in the serialized R-tree.\n", queryPoint.x, queryPoint.y);
    }
    else
    {
        printf("\nPoint (%f, %f) not found in the serialized R-tree.\n", queryPoint.x, queryPoint.y);
    }

    // Free memory (not implemented for simplicity)
    return 0;
}

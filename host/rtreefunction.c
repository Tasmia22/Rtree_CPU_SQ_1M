#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <common.h>
#include<float.h>



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

typedef struct SerializedNode
{
    int isLeaf;          // 1 if it's a leaf node, 0 if it's an internal node
    int count;           // Number of entries in the node
    MBR mbr;             // Bounding box for the node
    int children[16];    // Indices of child nodes (internal node), max FANOUT assumed to be 16
    Point points[16];    // Points (leaf node), max BUNDLEFACTOR assumed to be 16
} SerializedNode;

Node *copySubtree(Node *root);


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

// Function to create an R-tree node recursively
Node *createRTree(Point *ptArr, int low, int high)
{
    Node *root;

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
        //printf("\nCreating rtree mbr (%.1f,%.1f), (%.1f,%.1f) ",root->mbr.xmin,root->mbr.ymin,root->mbr.xmax,root->mbr.ymax );
    }

    return root;
}

// Function to print the R-tree (for debugging)
void printRTree(Node *node, int level)
{
    //printf("\n\nLevel=%d",level);
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
               // printf("\nSearching in mbr (%.1f,%.1f), (%.1f,%.1f) ",node->mbr.xmin,node->mbr.ymin,node->mbr.xmax,node->mbr.ymax );
                return true; // Point found in one of the children
            }
        }
        return false; // Point not found in any children
    }
}
// Function to count the number of nodes in a subtree
int countNodesInSubtree(Node *root)
{
    if (root == NULL)
        return 0;

    // Start with 1 for the current node
    int count = 1;

    if (!root->isLeaf)
    {
        // Recursively count children for internal nodes
        for (int i = 0; i < root->count; i++)
        {
            count += countNodesInSubtree(root->children[i]);
        }
    }

    return count;
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


void print_serialisedtree(int node_index, int depth, SerializedNode *serialized_tree) {
    if (node_index < 0) {
        printf("Invalid node index: %d\n", node_index);
        return;
    }

    // Print indentation for the current depth
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }

    // Print the current node's details
    printf("Node Index: %d | ", node_index);
    printf("MBR [xmin: %.1f, ymin: %.1f, xmax: %.1f, ymax: %.1f]",
           serialized_tree[node_index].mbr.xmin, serialized_tree[node_index].mbr.ymin,
           serialized_tree[node_index].mbr.xmax, serialized_tree[node_index].mbr.ymax);
    printf(" | isLeaf: %d | count: %d\n", serialized_tree[node_index].isLeaf, serialized_tree[node_index].count);

    if (serialized_tree[node_index].isLeaf) {
        // Print points for a leaf node
        for (int i = 0; i < serialized_tree[node_index].count; i++) {
            for (int j = 0; j < depth + 1; j++) {
                printf("  ");
            }
            //printf("Point (%.1f, %.1f)\n", serialized_tree[node_index].points[i].x,  serialized_tree[node_index].points[i].y);
        }
    } else {
        // Recursively print child nodes for an internal node
        for (int i = 0; i < serialized_tree[node_index].count; i++) {
            int child_index = serialized_tree[node_index].children[i];
            printf("\n");
            for (int j = 0; j < depth + 1; j++) {
                printf("  ");
            }
            //printf("Node index=%d and Child Node Index: %d\n", node_index,child_index);
            print_serialisedtree(child_index, depth + 1, serialized_tree);
        }
    }
}

// Function to copy a subtree rooted at a given node
Node *copySubtree(Node *root)
{
    if (root == NULL)
        return NULL;

    Node *newNode = (Node *)malloc(sizeof(Node));
    newNode->isLeaf = root->isLeaf;
    newNode->count = root->count;
    newNode->mbr = root->mbr;

    if (root->isLeaf)
    {
        // Copy points for leaf node
        newNode->points = (Point *)malloc(root->count * sizeof(Point));
        for (int i = 0; i < root->count; i++)
        {
            newNode->points[i] = root->points[i];
        }
    }
    else
    {
        // Copy children for internal node
        newNode->children = (Node **)malloc(root->count * sizeof(Node *));
        for (int i = 0; i < root->count; i++)
        {
            newNode->children[i] = copySubtree(root->children[i]);
        }
    }

    return newNode;
}



// Function to extract a subtree rooted at a given index in the R-tree
Node *extractSubtree(Node *root, int targetIndex, int *currentIndex)
{
    //printf("\n current Index %d", *currentIndex);
    if (root == NULL)
        return NULL;

    // If the current index matches the target index, return a copy of the subtree
    if (*currentIndex == targetIndex)
    {
        return copySubtree(root);
    }

    // Increment the current index
    (*currentIndex)++;

    if (!root->isLeaf)
    {
        // Traverse the children of the internal node
        for (int i = 0; i < root->count; i++)
        {
            Node *result = extractSubtree(root->children[i], targetIndex, currentIndex);
            if (result != NULL)
                return result;
        }
    }

    return NULL;
}
// Wrapper function to extract a subtree
Node *getSubtree(Node *root, int targetIndex)
{
    int currentIndex = 0;
    return extractSubtree(root, targetIndex, &currentIndex);
}


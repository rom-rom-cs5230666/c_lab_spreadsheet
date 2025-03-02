#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<math.h>
#include<time.h>

struct Sheet;
struct Cell;

int setarith(struct Cell* target_cell, int val1, int val2, int opcode);
int setfunc(struct Cell* target_cell, int* values, int count, int has_error, int opcode);
void sleep(int seconds);
void get_column_name(int col, char* buffer);

typedef struct DependencyNode {
    int row;
    int col;
    struct DependencyNode* next;
} DependencyNode;

struct Cell {
    int value;
    char* formula;
    int has_error;
    struct DependencyNode* depends_on;    // Cells that this cell depends on
    struct DependencyNode* dependents;    // Cells that depend on this cell
};


enum CommandType {
   CMD_CONTROL,     // w,a,s,d,q commands
   CMD_SETCONST,    // direct value assignment
   CMD_SETARITH,    // arithmetic operations
   CMD_SETFUNC,     // functions like MIN, MAX etc
   CMD_SETRANGE,    // range operations
   CMD_INVALID,      // for error handling
   CMD_DISABLE_OUTPUT, // to disable output
   CMD_ENABLE_OUTPUT, // to enable output
   CMD_SCROLL_TO    // to scroll to a specific cell
};

struct Command {
   enum CommandType type;
   char* args[4];   // Dynamic array to store arguments
};

struct Sheet {
   struct Cell** cells;
   int rows;
   int cols;
   int view_row;    // Top row of visible window
   int view_col;    // Leftmost column of visible window
   int output_enabled;    
   // No need for separate left,right,up,down as view_row and view_col define the window
};


// Helper function to calculate standard deviation
// Custom square root implementation using Newton's method
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helper function to add a dependency
void add_dependency(struct Cell* dependent, int dep_row, int dep_col) {
    // Check if dependency already exists
    DependencyNode* curr = dependent->depends_on;
    while (curr) {
        if (curr->row == dep_row && curr->col == dep_col) {
            return; // Dependency already exists
        }
        curr = curr->next;
    }
    
    // Create new dependency node
    DependencyNode* new_node = malloc(sizeof(DependencyNode));
    if (!new_node) return;
    
    new_node->row = dep_row;
    new_node->col = dep_col;
    new_node->next = dependent->depends_on;
    dependent->depends_on = new_node;
}

// Helper function to add a dependent
void add_dependent(struct Cell* dependency, int dep_row, int dep_col) {
    // Check if dependent already exists
    DependencyNode* curr = dependency->dependents;
    while (curr) {
        if (curr->row == dep_row && curr->col == dep_col) {
            return; // Dependent already exists
        }
        curr = curr->next;
    }
    
    // Create new dependent node
    DependencyNode* new_node = malloc(sizeof(DependencyNode));
    if (!new_node) return;
    
    new_node->row = dep_row;
    new_node->col = dep_col;
    new_node->next = dependency->dependents;
    dependency->dependents = new_node;
}

// Helper function to clear all dependencies for a cell
void clear_dependencies(struct Cell* cell) {
    // Remove this cell from all cells it depends on
    DependencyNode* curr = cell->depends_on;
    while (curr) {
        DependencyNode* dep_node = curr;
        curr = curr->next;
        
        // Free the dependency node
        free(dep_node);
    }
    cell->depends_on = NULL;
}

// Helper function to print dependencies
void print_dependencies(struct Sheet* sheet) {
    printf("\n--- Current Dependencies ---\n");
    for (int i = 0; i < sheet->rows; i++) {
        for (int j = 0; j < sheet->cols; j++) {
            char cell_name[10];
            get_column_name(j + 1, cell_name);
            printf("%s%d: ", cell_name, i + 1);
            
            // Print dependencies
            printf("depends on: ");
            DependencyNode* curr = sheet->cells[i][j].depends_on;
            if (!curr) {
                printf("none");
            } else {
                while (curr) {
                    char dep_name[10];
                    get_column_name(curr->col + 1, dep_name);
                    printf("%s%d ", dep_name, curr->row + 1);
                    curr = curr->next;
                }
            }
            
            // Print dependents
            printf(", dependents: ");
            curr = sheet->cells[i][j].dependents;
            if (!curr) {
                printf("none");
            } else {
                while (curr) {
                    char dep_name[10];
                    get_column_name(curr->col + 1, dep_name);
                    printf("%s%d ", dep_name, curr->row + 1);
                    curr = curr->next;
                }
            }
            printf("\n");
        }
    }
    printf("-------------------------\n\n");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double sqrt(double x) {
    if (x <= 0) return 0;
    double guess = x / 2;
    double prev;
    do {
        prev = guess;
        guess = (guess + x / guess) / 2;
    } while (prev - guess > 0.0001 || guess - prev > 0.0001);
    return guess;
}

// Custom sleep implementation using a busy-wait loop
void sleep(int seconds) {
    clock_t end_time = clock() + seconds * CLOCKS_PER_SEC;
    while (clock() < end_time) {
        // Busy wait
    }
}

// Helper function to calculate standard deviation
double calculate_stdev(int* values, int count) {
    if (count <= 1) return 0;
    
    // Calculate mean
    double mean = 0;
    for (int i = 0; i < count; i++) {
        mean += values[i];
    }
    mean /= count;
    
    // Calculate sum of squared differences
    double sum_sq_diff = 0;
    for (int i = 0; i < count; i++) {
        double diff = values[i] - mean;
        sum_sq_diff += diff * diff;
    }
    
    // Return standard deviation
    return sqrt(sum_sq_diff / (count - 1));
}

// Rest of setfunc remains the same
int setfunc(struct Cell* target_cell, int* values, int count, int has_error, int opcode) {
    if (has_error) {
        target_cell->has_error = 1;
        return 1;
    }


    if (count <= 0) return 1;  // Error: empty range
    
    int result = 0;
    double temp = 0;  // For average and stdev calculations
    
    switch(opcode) {
        case 1:  // MIN
            result = values[0];
            for (int i = 1; i < count; i++) {
                if (values[i] < result) {
                    result = values[i];
                }
            }
            break;
            
        case 2:  // MAX
            result = values[0];
            for (int i = 1; i < count; i++) {
                if (values[i] > result) {
                    result = values[i];
                }
            }
            break;
            
        case 3:  // AVG
            for (int i = 0; i < count; i++) {
                temp += values[i];
            }
            result = (int)(temp / count);
            break;
            
        case 4:  // SUM
            for (int i = 0; i < count; i++) {
                result += values[i];
            }
            break;
            
        case 5:  // STDEV
            result = (int)calculate_stdev(values, count);
            break;
            
        case 6:  // SLEEP
            if (count != 1) return 1;  // SLEEP takes exactly one value
            sleep(values[0]);  // Sleep for specified seconds
            result = values[0];
            break;
            
        default:
            return 1;  // Error: invalid operation
    }
    
    target_cell->value = result;
    target_cell->has_error = 0;
    free(target_cell->formula);
    target_cell->formula = NULL;
    
    return 0;
}
int setconst(char* cell_ref, char* value, struct Sheet* sheet) {
    int target_row = 0, target_col = 0;
    
    // Parse target cell reference (A1, B2, etc)
    int i = 0;
    while(cell_ref[i] >= 'A' && cell_ref[i] <= 'Z') {
        target_col = target_col * 26 + (cell_ref[i] - 'A' + 1);
        i++;
    }
    target_col--; // Convert to 0-based indexing
    target_row = atoi(cell_ref + i) - 1; // -1 for 0-based indexing
    
    // Validate target row and column
    if(target_row < 0 || target_row >= sheet->rows || 
       target_col < 0 || target_col >= sheet->cols) {
        return 1; // Error: invalid cell reference
    }

    // Clear existing dependencies
    clear_dependencies(&sheet->cells[target_row][target_col]);
    
    int val;
    int has_error = 0;
    
    // Check if value is a cell reference (starts with letter) or a number
    if(value[0] >= 'A' && value[0] <= 'Z') {
        // Parse source cell reference
        int source_row = 0, source_col = 0;
        i = 0;
        while(value[i] >= 'A' && value[i] <= 'Z') {
            source_col = source_col * 26 + (value[i] - 'A' + 1);
            i++;
        }
        source_col--; // Convert to 0-based indexing
        source_row = atoi(value + i) - 1; // -1 for 0-based indexing
        
        // Validate source row and column
        if(source_row < 0 || source_row >= sheet->rows || 
           source_col < 0 || source_col >= sheet->cols) {
            return 1; // Error: invalid cell reference
        }
        
        val = sheet->cells[source_row][source_col].value;
        has_error = sheet->cells[source_row][source_col].has_error;

        // Set up dependency
        add_dependency(&sheet->cells[target_row][target_col], source_row, source_col);
        add_dependent(&sheet->cells[source_row][source_col], target_row, target_col);
        
        // Store formula
        free(sheet->cells[target_row][target_col].formula);
        sheet->cells[target_row][target_col].formula = strdup(value);

    } else {
        // Treat as integer constant
        val = atoi(value);
        has_error = 0;

        // Clear formula
        free(sheet->cells[target_row][target_col].formula);
        sheet->cells[target_row][target_col].formula = NULL;
    }
    
    // Set the value
    sheet->cells[target_row][target_col].value = val;
    sheet->cells[target_row][target_col].has_error = has_error;
    // Clear any existing formula
    free(sheet->cells[target_row][target_col].formula);
    sheet->cells[target_row][target_col].formula = NULL;
    
    return 0;
 }

// Converts column number to Excel-style column name (A, B, C, ... AA, AB, etc)
void get_column_name(int col, char* buffer) {
    // Convert to 0-based indexing
    col--;
    
    if (col < 26) {  // Single letter
        buffer[0] = 'A' + col;
        buffer[1] = '\0';
        return;
    }
    
    // For columns beyond Z
    int first = col / 26;
    int second = col % 26;
    
    if (first <= 26) {  // Two letters
        buffer[0] = 'A' + first - 1;
        buffer[1] = 'A' + second;
        buffer[2] = '\0';
    } else {  // Three letters needed
        int third = second;
        second = first % 26;
        first = first / 26;
        buffer[0] = 'A' + first - 1;
        buffer[1] = 'A' + second - 1;
        buffer[2] = 'A' + third;
        buffer[3] = '\0';
    }
}

int display(struct Sheet* sheet) {
    if (!sheet->output_enabled) {
        return 0;
    }

   char col_name[4];
   
   // Determine visible boundaries (10x10 window)
   int end_row = sheet->view_row + 10;
   int end_col = sheet->view_col + 10;
   if (end_row > sheet->rows) end_row = sheet->rows;
   if (end_col > sheet->cols) end_col = sheet->cols;
   
   // Print column headers for visible columns
   printf("    ");
   for(int j = sheet->view_col; j < end_col; j++) {
       get_column_name(j + 1, col_name);
       printf("%-4s ", col_name);
   }
   printf("\n");
   
   // Print visible rows
   for(int i = sheet->view_row; i < end_row; i++) {
       printf("%-3d ", i + 1);
       for(int j = sheet->view_col; j < end_col; j++) {
            if (sheet->cells[i][j].has_error) {
                printf("%-4s", "ERR");
            } else {
                printf("%-4d ", sheet->cells[i][j].value);
            }
       }
       printf("\n");
   }
   return 0;
}

int scroll_to(char* cell_ref, struct Sheet* sheet) {
    int target_row = 0, target_col = 0;
    
    // Parse cell reference (A1, B2, etc)
    int i = 0;
    while(cell_ref[i] >= 'A' && cell_ref[i] <= 'Z') {
        target_col = target_col * 26 + (cell_ref[i] - 'A' + 1);
        i++;
    }
    target_col--; // Convert to 0-based indexing
    target_row = atoi(cell_ref + i) - 1; // -1 for 0-based indexing
    
    // Validate target row and column
    if(target_row < 0 || target_row >= sheet->rows || 
       target_col < 0 || target_col >= sheet->cols) {
        return 1; // Error: invalid cell reference
    }
    
    // Update view position
    sheet->view_row = target_row;
    sheet->view_col = target_col;
    
    return 0;
}

int control(char* input, struct Sheet* sheet) {
    switch (input[0]) {
        case 'w':  // Move up
            if (sheet->view_row >= 10) {
                sheet->view_row -= 10;  // Move up by 10 rows
            } else {
                sheet->view_row = 0;  // Ensure no negative index
            }
            break;

        case 's':  // Move down
            if (sheet->view_row + 20 <= sheet->rows) {
                sheet->view_row += 10;  // Move down by 10 rows
            } else if (sheet->rows > 10) {
                sheet->view_row = sheet->rows - 10;  // Ensure last 10 rows are displayed
            }
            break;

        case 'a':  // Move left
            if (sheet->view_col >= 10) {
                sheet->view_col -= 10;  // Move left by 10 columns
            } else {
                sheet->view_col = 0;  // Ensure no negative index
            }
            break;

        case 'd':  // Move right
            if (sheet->view_col + 20 <= sheet->cols) {
                sheet->view_col += 10;  // Move right by 10 columns
            } else if (sheet->cols > 10) {
                sheet->view_col = sheet->cols - 10;  // Ensure last 10 columns are displayed
            }
            break;

        default:  // Invalid input
            return 1;
    }
    return 0;  // Success
}

int setarith(struct Cell* target_cell, int val1, int val2, int opcode) {
    int result;
    
    switch(opcode) {
        case 1:  // Addition
            result = val1 + val2;
            break;
        case 2:  // Subtraction
            result = val1 - val2;
            break;
        case 3:  // Multiplication
            result = val1 * val2;
            break;
        case 4:  // Division
            if(val2 == 0) {
                target_cell->has_error = 1;
                return 1;  // Error: division by zero
            }
            result = val1 / val2;
            break;
        default:
            return 1;  // Error: invalid operation
    }
    
    target_cell->value = result;
    target_cell->has_error = 0;
    free(target_cell->formula);
    target_cell->formula = NULL;
    
    return 0;
}

int evaluate_cell(struct Sheet* sheet, int row, int col) {
    struct Cell* cell = &sheet->cells[row][col];
    
    // If no formula, nothing to evaluate
    if (!cell->formula) return 0;
    
    char* formula = strdup(cell->formula);
    if (!formula) return 1;
    
    int result = 0;
    
    // Check if it's a simple cell reference (A1, B2, etc.)
    if (formula[0] >= 'A' && formula[0] <= 'Z' && strchr(formula, '+') == NULL && 
        strchr(formula, '-') == NULL && strchr(formula, '*') == NULL && 
        strchr(formula, '/') == NULL && strchr(formula, '(') == NULL) {
        
        // Parse the cell reference
        int source_row = 0, source_col = 0;
        int i = 0;
        while (formula[i] >= 'A' && formula[i] <= 'Z') {
            source_col = source_col * 26 + (formula[i] - 'A' + 1);
            i++;
        }
        source_col--; // Convert to 0-based indexing
        source_row = atoi(formula + i) - 1; // -1 for 0-based indexing
        
        // Validate source row and column
        if (source_row < 0 || source_row >= sheet->rows || 
            source_col < 0 || source_col >= sheet->cols) {
            free(formula);
            return 1; // Error: invalid cell reference
        }
        
        // Set the value from the referenced cell
        cell->value = sheet->cells[source_row][source_col].value;
        free(formula);
        return 0;
    }
    
    // Check for arithmetic operation
    char* op_ptr = NULL;
    if ((op_ptr = strchr(formula, '+')) || 
        (op_ptr = strchr(formula, '-')) || 
        (op_ptr = strchr(formula, '*')) || 
        (op_ptr = strchr(formula, '/'))) {
        
        char op = *op_ptr;
        *op_ptr = '\0'; // Split the formula at the operator
        
        char* left = formula;
        char* right = op_ptr + 1;
        
        // Get values for both operands
        int val1, val2;
        
        // Left operand
        if (left[0] >= 'A' && left[0] <= 'Z') {
            // Parse cell reference
            int source_row = 0, source_col = 0;
            int i = 0;
            while (left[i] >= 'A' && left[i] <= 'Z') {
                source_col = source_col * 26 + (left[i] - 'A' + 1);
                i++;
            }
            source_col--; // Convert to 0-based indexing
            source_row = atoi(left + i) - 1; // -1 for 0-based indexing
            
            // Validate source row and column
            if (source_row < 0 || source_row >= sheet->rows || 
                source_col < 0 || source_col >= sheet->cols) {
                free(formula);
                return 1; // Error: invalid cell reference
            }
            
            val1 = sheet->cells[source_row][source_col].value;
        } else {
            val1 = atoi(left);
        }
        
        // Right operand
        if (right[0] >= 'A' && right[0] <= 'Z') {
            // Parse cell reference
            int source_row = 0, source_col = 0;
            int i = 0;
            while (right[i] >= 'A' && right[i] <= 'Z') {
                source_col = source_col * 26 + (right[i] - 'A' + 1);
                i++;
            }
            source_col--; // Convert to 0-based indexing
            source_row = atoi(right + i) - 1; // -1 for 0-based indexing
            
            // Validate source row and column
            if (source_row < 0 || source_row >= sheet->rows || 
                source_col < 0 || source_col >= sheet->cols) {
                free(formula);
                return 1; // Error: invalid cell reference
            }
            
            val2 = sheet->cells[source_row][source_col].value;
        } else {
            val2 = atoi(right);
        }
        
        // Perform the arithmetic operation
        int opcode;
        switch (op) {
            case '+': opcode = 1; break;
            case '-': opcode = 2; break;
            case '*': opcode = 3; break;
            case '/': opcode = 4; break;
            default:
                free(formula);
                return 1; // Error: invalid operator
        }
        
        if (setarith(cell, val1, val2, opcode) != 0) {
            free(formula);
            return 1; // Error in arithmetic operation
        }
        
        free(formula);
        return 0;
    }
    
    // Check for function calls
    char* open_paren = strchr(formula, '(');
    char* close_paren = strrchr(formula, ')');
    
    if (open_paren && close_paren) {
        // Extract function name
        int func_len = open_paren - formula;
        char func_name[16] = {0}; // Buffer for function name
        strncpy(func_name, formula, func_len);
        
        // Extract arguments
        *close_paren = '\0'; // Terminate at close parenthesis
        char* args = open_paren + 1;
        
        // Handle SLEEP function separately
        if (strcmp(func_name, "SLEEP") == 0) {
            int sleep_time = atoi(args);
            if (sleep_time <= 0) {
                free(formula);
                return 1; // Error: invalid sleep time
            }
            
            sleep(sleep_time);
            cell->value = sleep_time;
            free(formula);
            return 0;
        }
        
        // Handle other functions
        int opcode;
        if (strcmp(func_name, "MIN") == 0) opcode = 1;
        else if (strcmp(func_name, "MAX") == 0) opcode = 2;
        else if (strcmp(func_name, "AVG") == 0) opcode = 3;
        else if (strcmp(func_name, "SUM") == 0) opcode = 4;
        else if (strcmp(func_name, "STDEV") == 0) opcode = 5;
        else {
            free(formula);
            return 1; // Error: unknown function
        }
        
        // Parse the range
        char* colon = strchr(args, ':');
        int start_row, start_col, end_row, end_col;
        
        // Parse start cell
        int i = 0;
        start_col = 0;
        while (args[i] >= 'A' && args[i] <= 'Z') {
            start_col = start_col * 26 + (args[i] - 'A' + 1);
            i++;
        }
        start_col--; // Convert to 0-based indexing
        start_row = atoi(args + i) - 1; // -1 for 0-based indexing
        
        // If there's a range (colon found), parse end cell
        if (colon) {
            char* end_cell = colon + 1;
            i = 0;
            end_col = 0;
            while (end_cell[i] >= 'A' && end_cell[i] <= 'Z') {
                end_col = end_col * 26 + (end_cell[i] - 'A' + 1);
                i++;
            }
            end_col--; // Convert to 0-based indexing
            end_row = atoi(end_cell + i) - 1; // -1 for 0-based indexing
        } else {
            // Single cell case
            end_row = start_row;
            end_col = start_col;
        }
        
        // Validate range
        if (start_row < 0 || start_row >= sheet->rows || 
            start_col < 0 || start_col >= sheet->cols ||
            end_row < 0 || end_row >= sheet->rows ||
            end_col < 0 || end_col >= sheet->cols ||
            end_row < start_row || end_col < start_col) {
            free(formula);
            return 1; // Error: invalid range
        }
        
        // Collect values from range
        int rows = end_row - start_row + 1;
        int cols = end_col - start_col + 1;
        int count = rows * cols;
        int* values = malloc(count * sizeof(int));
        int has_error = 0;
        if (!values) {
            free(formula);
            return 1; // Error: memory allocation failed
        }
        
        int idx = 0;
        for (int r = start_row; r <= end_row; r++) {
            for (int c = start_col; c <= end_col; c++) {
                values[idx++] = sheet->cells[r][c].value;
            }
        }
        
        // Apply function
        if (setfunc(cell, values, count, has_error, opcode) != 0) {
            free(values);
            free(formula);
            return 1; // Error in function application
        }
        
        free(values);
        free(formula);
        return 0;
    }
    
    free(formula);
    return 1; // No recognizable formula pattern
}

// Function to detect cycles in the dependency graph using DFS
int detect_cycle(struct Sheet* sheet, int row, int col, int* visited, int* in_stack) {
    int cell_idx = row * sheet->cols + col;
    
    // If already completely visited, no cycle here
    if (visited[cell_idx]) return 0;
    
    // Mark as visited and in current path
    visited[cell_idx] = 1;
    in_stack[cell_idx] = 1;
    
    // Visit all dependencies
    DependencyNode* curr = sheet->cells[row][col].depends_on;
    while (curr) {
        int dep_idx = curr->row * sheet->cols + curr->col;
        
        // If in current path, cycle detected
        if (in_stack[dep_idx]) return 1;
        
        // If not visited yet, recurse
        if (!visited[dep_idx]) {
            if (detect_cycle(sheet, curr->row, curr->col, visited, in_stack)) {
                return 1;
            }
        }
        
        curr = curr->next;
    }
    
    // Remove from current path
    in_stack[cell_idx] = 0;
    return 0;
}

// Topological sort to determine evaluation order
void topological_sort_visit(struct Sheet* sheet, int row, int col, int* visited, int** order, int* order_idx) {
    int cell_idx = row * sheet->cols + col;
    if (visited[cell_idx]) return;
    
    visited[cell_idx] = 1;
    
    // Visit all dependents first
    DependencyNode* curr = sheet->cells[row][col].dependents;
    while (curr) {
        if (!visited[curr->row * sheet->cols + curr->col]) {
            topological_sort_visit(sheet, curr->row, curr->col, visited, order, order_idx);
        }
        curr = curr->next;
    }
    
    // Add current cell to the order
    (*order)[*order_idx * 2] = row;
    (*order)[*order_idx * 2 + 1] = col;
    (*order_idx)++;
}

// Function to update all cells that depend on the given cell
int update_dependencies(struct Sheet* sheet, int row, int col) {
    int total_cells = sheet->rows * sheet->cols;
    
    // Arrays to track visited cells and build the evaluation order
    int* visited = calloc(total_cells, sizeof(int));
    int* eval_order = malloc(total_cells * 2 * sizeof(int)); // To store (row,col) pairs
    
    if (!visited || !eval_order) {
        free(visited);
        free(eval_order);
        return 1; // Memory allocation failed
    }
    
    int eval_count = 0;
    
    // Recursive function to collect all dependent cells
    void collect_dependents(int r, int c) {
        int cell_idx = r * sheet->cols + c;
        if (visited[cell_idx]) return;
        
        visited[cell_idx] = 1;
        
        // Process all cells that depend on this one
        DependencyNode* dep = sheet->cells[r][c].dependents;
        while (dep) {
            collect_dependents(dep->row, dep->col);
            dep = dep->next;
        }
        
        // Add this cell to the evaluation order (except the starting cell)
        if (r != row || c != col) {
            eval_order[eval_count * 2] = r;
            eval_order[eval_count * 2 + 1] = c;
            eval_count++;
        }
    }
    
    // Start the collection from the changed cell
    collect_dependents(row, col);
    
    // Now evaluate all dependent cells in REVERSE ORDER
    // This is crucial: we need to start from cells that depend directly on the changed cell
    for (int i = eval_count - 1; i >= 0; i--) {
        int r = eval_order[i * 2];
        int c = eval_order[i * 2 + 1];
        
        //printf("Updating dependent cell: %c%d\n", 'A' + c, r + 1); // Debug print
        evaluate_cell(sheet, r, c);
    }
    
    free(visited);
    free(eval_order);
    return 0;
}

int handle_setfunc(struct Sheet* sheet, char* target_cell_ref, char* range_str, int opcode) {
    int target_row = 0, target_col = 0;
    
    // Parse target cell reference
    int i = 0;
    while(target_cell_ref[i] >= 'A' && target_cell_ref[i] <= 'Z') {
        target_col = target_col * 26 + (target_cell_ref[i] - 'A' + 1);
        i++;
    }
    target_col--; // Convert to 0-based indexing
    target_row = atoi(target_cell_ref + i) - 1; // -1 for 0-based indexing
    
    // Validate target cell
    if(target_row < 0 || target_row >= sheet->rows || 
       target_col < 0 || target_col >= sheet->cols) {
        return 1; // Error: invalid cell reference
    }
    
    // Clear existing dependencies
    clear_dependencies(&sheet->cells[target_row][target_col]);
    
    // Special handling for SLEEP
    if (opcode == 6) {
        int sleep_time = atoi(range_str);
        if (sleep_time <= 0) {
            return 1; // Error: invalid sleep time
        }
        
        // Store formula
        char formula[256];
        sprintf(formula, "SLEEP(%s)", range_str);
        free(sheet->cells[target_row][target_col].formula);
        sheet->cells[target_row][target_col].formula = strdup(formula);
        
        sleep(sleep_time);
        sheet->cells[target_row][target_col].value = sleep_time;
        
        // Debug: Print dependencies
        //print_dependencies(sheet);
        
        // No need to update dependencies for SLEEP
        return 0;
    }
    
    // Handle range-based functions
    char* colon = strchr(range_str, ':');
    int start_row = 0, start_col = 0, end_row = 0, end_col = 0;
    
    // Parse start cell
    i = 0;
    while(range_str[i] >= 'A' && range_str[i] <= 'Z') {
        start_col = start_col * 26 + (range_str[i] - 'A' + 1);
        i++;
    }
    start_col--; // Convert to 0-based indexing
    start_row = atoi(range_str + i) - 1; // -1 for 0-based indexing
    
    // If there's a range (colon found), parse end cell
    if (colon) {
        char* end_cell = colon + 1;
        i = 0;
        while(end_cell[i] >= 'A' && end_cell[i] <= 'Z') {
            end_col = end_col * 26 + (end_cell[i] - 'A' + 1);
            i++;
        }
        end_col--; // Convert to 0-based indexing
        end_row = atoi(end_cell + i) - 1; // -1 for 0-based indexing
    } else {
        // Single cell case
        end_row = start_row;
        end_col = start_col;
    }
    
    // Validate range
    if (start_row < 0 || start_row >= sheet->rows || 
        start_col < 0 || start_col >= sheet->cols ||
        end_row < 0 || end_row >= sheet->rows ||
        end_col < 0 || end_col >= sheet->cols ||
        end_row < start_row || end_col < start_col) {
        return 1; // Error: invalid range
    }
    
    // Store formula
    char func_name[10];
    switch (opcode) {
        case 1: strcpy(func_name, "MIN"); break;
        case 2: strcpy(func_name, "MAX"); break;
        case 3: strcpy(func_name, "AVG"); break;
        case 4: strcpy(func_name, "SUM"); break;
        case 5: strcpy(func_name, "STDEV"); break;
        default: return 1; // Error: invalid function
    }
    
    char formula[256];
    sprintf(formula, "%s(%s)", func_name, range_str);
    free(sheet->cells[target_row][target_col].formula);
    sheet->cells[target_row][target_col].formula = strdup(formula);
    
    // Set up dependencies for each cell in the range
    for (int r = start_row; r <= end_row; r++) {
        for (int c = start_col; c <= end_col; c++) {
            add_dependency(&sheet->cells[target_row][target_col], r, c);
            add_dependent(&sheet->cells[r][c], target_row, target_col);
        }
    }
    
    // Collect values from range
    int rows = end_row - start_row + 1;
    int cols = end_col - start_col + 1;
    int count = rows * cols;
    int* values = malloc(count * sizeof(int));
    int has_error = 0;
    if (!values) {
        return 1; // Error: memory allocation failed
    }
    
    int idx = 0;
    for (int r = start_row; r <= end_row; r++) {
        for (int c = start_col; c <= end_col; c++) {
            values[idx++] = sheet->cells[r][c].value;
        }
    }
    
    // Apply function
    if (setfunc(&sheet->cells[target_row][target_col], values, count, has_error, opcode) != 0) {
        free(values);
        return 1; // Error: function application failed
    }
    
    free(values);
    
    // Debug: Print dependencies
    //print_dependencies(sheet);
    
    // Update dependent cells
    update_dependencies(sheet, target_row, target_col);
    
    return 0;
}int handle_setarith(struct Sheet* sheet, char* target_cell_ref, char* left_operand, char* right_operand, char op) {
    int target_row = 0, target_col = 0;
    
    // Parse target cell reference
    int i = 0;
    while(target_cell_ref[i] >= 'A' && target_cell_ref[i] <= 'Z') {
        target_col = target_col * 26 + (target_cell_ref[i] - 'A' + 1);
        i++;
    }
    target_col--; // Convert to 0-based indexing
    target_row = atoi(target_cell_ref + i) - 1; // -1 for 0-based indexing
    
    // Validate target cell
    if(target_row < 0 || target_row >= sheet->rows || 
       target_col < 0 || target_col >= sheet->cols) {
        return 1; // Error: invalid cell reference
    }
    
    // Clear existing dependencies
    clear_dependencies(&sheet->cells[target_row][target_col]);
    
    // Get values for both operands (could be cell refs or constants)
    int val1, val2;
    int left_row = -1, left_col = 0;  // Initialize to 0, not -1
    int right_row = -1, right_col = 0;  // Initialize to 0, not -1
    
    if(left_operand[0] >= 'A' && left_operand[0] <= 'Z') {
        // First arg is cell reference
        i = 0;
        while(left_operand[i] >= 'A' && left_operand[i] <= 'Z') {
            left_col = left_col * 26 + (left_operand[i] - 'A' + 1);
            i++;
        }
        left_col--;
        left_row = atoi(left_operand + i) - 1;
        if(left_row < 0 || left_row >= sheet->rows || left_col < 0 || left_col >= sheet->cols) {
            return 1; // Error: invalid cell reference
        }
        val1 = sheet->cells[left_row][left_col].value;
        
        // Add dependency relationship
        add_dependency(&sheet->cells[target_row][target_col], left_row, left_col);
        add_dependent(&sheet->cells[left_row][left_col], target_row, target_col);
    } else {
        val1 = atoi(left_operand);
    }
    
    if(right_operand[0] >= 'A' && right_operand[0] <= 'Z') {
        // Second arg is cell reference
        i = 0;
        while(right_operand[i] >= 'A' && right_operand[i] <= 'Z') {
            right_col = right_col * 26 + (right_operand[i] - 'A' + 1);
            i++;
        }
        right_col--;
        right_row = atoi(right_operand + i) - 1;
        if(right_row < 0 || right_row >= sheet->rows || right_col < 0 || right_col >= sheet->cols) {
            return 1; // Error: invalid cell reference
        }
        val2 = sheet->cells[right_row][right_col].value;
        
        // Add dependency relationship
        add_dependency(&sheet->cells[target_row][target_col], right_row, right_col);
        add_dependent(&sheet->cells[right_row][right_col], target_row, target_col);
    } else {
        val2 = atoi(right_operand);
    }
    
    // Store the formula
    char formula[256];
    sprintf(formula, "%s%c%s", left_operand, op, right_operand);
    free(sheet->cells[target_row][target_col].formula);
    sheet->cells[target_row][target_col].formula = strdup(formula);
    
    // Get operator and perform operation
    int opcode;
    switch(op) {
        case '+': opcode = 1; break;
        case '-': opcode = 2; break;
        case '*': opcode = 3; break;
        case '/': opcode = 4; break;
        default: return 1; // Error: invalid operator
    }
    
    if (setarith(&sheet->cells[target_row][target_col], val1, val2, opcode) != 0) {
        return 1; // Error: arithmetic error
    }
    
    // Debug: Print dependencies
    //print_dependencies(sheet);
    
    // Update dependent cells
    update_dependencies(sheet, target_row, target_col);
    
    return 0;
}



struct Command* parse_input(char* input) {
    struct Command* cmd = malloc(sizeof(struct Command));
    if (cmd == NULL) return NULL;
    
    for(int i = 0; i < 4; i++) {
        cmd->args[i] = NULL;
    }
 
    // Handle single-character control commands
    if (strlen(input) == 1) {
        switch(input[0]) {
            case 'w':
            case 'a':
            case 's':
            case 'd':
            case 'q':
                cmd->type = CMD_CONTROL;
                cmd->args[0] = malloc(strlen(input) + 1);
                if (cmd->args[0] != NULL) {
                    strcpy(cmd->args[0], input);
                }
                return cmd;
        }
    }

    if (strcmp(input, "disable_output") == 0) {
        cmd->type = CMD_DISABLE_OUTPUT;
        return cmd;
    }

    if (strcmp(input, "enable_output") == 0) {
        cmd->type = CMD_ENABLE_OUTPUT;
        return cmd;
    }

    if (strncmp(input, "scroll_to ", 10) == 0) {
        cmd->type = CMD_SCROLL_TO;
        char* cell_ref = input + 10;  // Skip "scroll_to " prefix

        // Trim leading/trailing whitespace
        while(*cell_ref && isspace(*cell_ref)) cell_ref++;
        char* end = cell_ref + strlen(cell_ref) - 1;
        while(end > cell_ref && isspace(*end)) *end-- = '\0';

        if (strlen(cell_ref) > 0) {
            cmd->args[0] = malloc(strlen(cell_ref) + 1);
            if (cmd->args[0] != NULL) {
                strcpy(cmd->args[0], cell_ref);
                return cmd;
            }
        }
        cmd->type = CMD_INVALID;
        return cmd;
    }
 
    // Check for assignment (format: A1=5 or A1=B2 or A1=MIN(B1:C1))
    char* equals = strchr(input, '=');
    if (equals != NULL) {
        *equals = '\0';  // Split at equals sign
        char* cell = input;
        char* value = equals + 1;
        
        // Trim leading/trailing whitespace
        while(*cell && isspace(*cell)) cell++;
        while(*value && isspace(*value)) value++;
        char* end = cell + strlen(cell) - 1;
        while(end > cell && isspace(*end)) *end-- = '\0';
        end = value + strlen(value) - 1;
        while(end > value && isspace(*end)) *end-- = '\0';
 
        // Check for function calls first (MIN, MAX, etc.)
        char* open_paren = strchr(value, '(');
        char* close_paren = strchr(value, ')');
        if (open_paren && close_paren && open_paren < close_paren) {
            // Extract function name
            int func_len = open_paren - value;
            char func_name[10] = {0};  // Large enough for any function name
            strncpy(func_name, value, func_len);
            
            // Get range between parentheses
            char* arg = open_paren + 1;
            *close_paren = '\0';
            
            cmd->type = CMD_SETFUNC;
            cmd->args[0] = strdup(cell);     // Target cell
            //cmd->args[1] = strdup(range);    // Range
            
            if (strcmp(func_name, "SLEEP") == 0) {
                for (int i = 0; arg[i] != '\0'; i++) {
                    if (!isdigit(arg[i])) {
                        printf("SLEEP expects a positive integer as an argument!\n");
                        cmd->type = CMD_INVALID;
                        return cmd;
                    }
                }

                cmd->args[1] = strdup(arg);
                cmd->args[2] = strdup("6");
                return cmd;
            }

            cmd->args[1] = strdup(arg);


            // Determine function type
            if (strcmp(func_name, "MIN") == 0) cmd->args[2] = strdup("1");
            else if (strcmp(func_name, "MAX") == 0) cmd->args[2] = strdup("2");
            else if (strcmp(func_name, "AVG") == 0) cmd->args[2] = strdup("3");
            else if (strcmp(func_name, "SUM") == 0) cmd->args[2] = strdup("4");
            else if (strcmp(func_name, "STDEV") == 0) cmd->args[2] = strdup("5");
            //else if (strcmp(func_name, "SLEEP") == 0) cmd->args[2] = strdup("6");
            else {
                cmd->type = CMD_INVALID;
                return cmd;
            }
            return cmd;
        }
 
        // Look for arithmetic operators
        char* operator = strpbrk(value, "+-*/");
        if (operator != NULL) {
            printf("Found operator: %c\n", *operator);
            // Store the operator character
            char opcode = *operator;
            *operator = '\0';  // Split at operator
            printf("Cell: %s, First operand: %s, Second operand: %s\n",cell,value,operator+1);
            cmd->type = CMD_SETARITH;
            cmd->args[0] = malloc(strlen(cell) + 1);
            cmd->args[1] = malloc(strlen(value) + 1);
            cmd->args[2] = malloc(strlen(operator+1)+1);
            cmd->args[3] = malloc(2);  // +2 for operator and null terminator
            
            if(cmd->args[0] && cmd->args[1] && cmd->args[2] && cmd->args[3]) {
                strcpy(cmd->args[0], cell);     // Target cell
                strcpy(cmd->args[1], value);    // First operand
                strcpy(cmd->args[2], operator + 1); // Second operand
                
                // Append the operator to args[2]
                cmd->args[3][0]= opcode;
                cmd->args[3][1] = '\0';
                return cmd;
            } else {
                printf("Memory allocation failed\n");
                // Clean up if any allocation failed
                free(cmd->args[0]);
                free(cmd->args[1]);
                free(cmd->args[2]);
                free(cmd->args[3]);
                cmd->args[0] = cmd->args[1] = cmd->args[2]= cmd->args[3] = NULL;
                cmd->type = CMD_INVALID;
                
                // Restore the operator character in the string
                *operator = opcode;
                printf("Successfully created arithmetic command\n");
                return cmd;
            }
        } else if (strlen(value) > 0) {
            // Regular constant or cell reference assignment
            cmd->type = CMD_SETCONST;
            cmd->args[0] = malloc(strlen(cell) + 1);
            cmd->args[1] = malloc(strlen(value) + 1);
            if (cmd->args[0] && cmd->args[1]) {
                strcpy(cmd->args[0], cell);
                strcpy(cmd->args[1], value);
                return cmd;
            }
        }
    }
 
    cmd->type = CMD_INVALID;
    return cmd;
}

void update_dependent_cells(struct Sheet* sheet, int changed_row, int changed_col) {}
int main(int argc, char *argv[]) {
   struct Sheet* sheet = NULL;
   int state = 0;  // Will store this in sheet later
   char input[1024];

   if (state == 0) {
       // Original initialization code
       if (argc != 3) {
           printf("Usage: ./sheet <rows> <cols>\n"); 
           return 1;
       }

       sheet = malloc(sizeof(struct Sheet));
       if (sheet == NULL) {
           printf("Memory allocation failed\n");
           return 1;
       }

       sheet->rows = atoi(argv[1]);
       sheet->cols = atoi(argv[2]);
       sheet->view_row = 0;
       sheet->view_col = 0;
       sheet->output_enabled = 1;

       // Allocate array of row pointers
       sheet->cells = malloc(sheet->rows * sizeof(struct Cell*));
       if (sheet->cells == NULL) {
           free(sheet);
           printf("Memory allocation failed\n");
           return 1;
       }

       // Allocate each row and initialize cells to 0
       for (int i = 0; i < sheet->rows; i++) {
           sheet->cells[i] = malloc(sheet->cols * sizeof(struct Cell));
           if (sheet->cells[i] == NULL) {
               // Free previously allocated rows
               for (int j = 0; j < i; j++) {
                   free(sheet->cells[j]);
               }
               free(sheet->cells);
               free(sheet);
               printf("Memory allocation failed\n");
               return 1;
           }
           
           // Initialize each cell in this row to 0
           for (int j = 0; j < sheet->cols; j++) {
               sheet->cells[i][j].value = 0;
               sheet->cells[i][j].formula = NULL;
               sheet->cells[i][j].has_error = 0;
               sheet->cells[i][j].depends_on = NULL;
               sheet->cells[i][j].dependents = NULL;
           }
       }
       
       state = 1;
       display(sheet);  // Initial display after initialization
   }

   // Main program loop
   while(state == 1) {
       printf("> ");
       
       if (fgets(input, sizeof(input), stdin) == NULL) {
           break;
       }
       
       // Remove newline if present
       input[strcspn(input, "\n")] = 0;

       struct Command* cmd = parse_input(input);
       if (cmd == NULL) {
           printf("Memory allocation failed\n");
           continue;
       }

       // Handle command
       switch(cmd->type) {
            case CMD_CONTROL:
               if (cmd->args[0][0] == 'q') {
                   state = 0;  // Exit program
               }
               else {
                   control(cmd->args[0], sheet);
                   display(sheet);  // Display after control command
               }
               break;

            case CMD_DISABLE_OUTPUT:
               sheet->output_enabled = 0;
               break;

            case CMD_ENABLE_OUTPUT:
               sheet->output_enabled = 1;
               break;

            case CMD_SCROLL_TO:
               if (scroll_to(cmd->args[0], sheet) != 0) {
                printf("Invalid cell reference\n");
               }

               else if (sheet->output_enabled) {
                display(sheet);
               }

               break;

            case CMD_SETCONST:
               if (setconst(cmd->args[0], cmd->args[1], sheet) != 0) {
                   printf("Invalid cell reference or value\n");
               }
               display(sheet);
               break;
               case CMD_SETARITH: {
                // Parse target cell
                int target_row = 0, target_col = 0;
                int i = 0;
                while(cmd->args[0][i] >= 'A' && cmd->args[0][i] <= 'Z') {
                    target_col = target_col * 26 + (cmd->args[0][i] - 'A' + 1);
                    i++;
                }
                target_col--; // Convert to 0-based indexing
                target_row = atoi(cmd->args[0] + i) - 1;
                
                // Validate target cell
                if(target_row < 0 || target_row >= sheet->rows || 
                   target_col < 0 || target_col >= sheet->cols) {
                    printf("Invalid target cell reference\n");
                    break;
                }
                
                // Get values for both operands (could be cell refs or constants)
                int val1, val2;
                int has_error1 = 0, has_error2 = 0;

                if(cmd->args[1][0] >= 'A' && cmd->args[1][0] <= 'Z') {
                    // First arg is cell reference
                    int row = 0, col = 0;
                    i = 0;
                    while(cmd->args[1][i] >= 'A' && cmd->args[1][i] <= 'Z') {
                        col = col * 26 + (cmd->args[1][i] - 'A' + 1);
                        i++;
                    }
                    col--;
                    row = atoi(cmd->args[1] + i) - 1;
                    if(row < 0 || row >= sheet->rows || col < 0 || col >= sheet->cols) {
                        printf("Invalid cell reference in first operand\n");
                        break;
                    }
                    val1 = sheet->cells[row][col].value;
                    has_error1 = sheet->cells[row][col].has_error;
                    //printf("%d",val1);
                } else {
                    val1 = atoi(cmd->args[1]);
                    has_error1 = 0;

                }
                
                if(cmd->args[2][0] >= 'A' && cmd->args[2][0] <= 'Z') {
                    // Second arg is cell reference
                    int row = 0, col = 0;
                    i = 0;
                    while(cmd->args[2][i] >= 'A' && cmd->args[2][i] <= 'Z') {
                        col = col * 26 + (cmd->args[2][i] - 'A' + 1);
                        i++;
                    }
                    col--;
                    row = atoi(cmd->args[2] + i) - 1;
                    if(row < 0 || row >= sheet->rows || col < 0 || col >= sheet->cols) {
                        printf("Invalid cell reference in second operand\n");
                        break;
                    }
                    val2 = sheet->cells[row][col].value;
                    has_error2 = sheet->cells[row][col].has_error;
                    //printf("%d",val2);
                } else {
                    val2 = atoi(cmd->args[2]);
                    has_error2 = 0;
                }

                if (has_error1 || has_error2) {
                    sheet->cells[target_row][target_col].has_error = 1;
                    display(sheet);
                    break;
                }
                
                // In the CMD_SETARITH case:
        // Get operator (stored at end of args[2])
        char op = cmd->args[3][0];
        int opcode;
        
        switch(op) {
            case '+': opcode = 1; break;
            case '-': opcode = 2; break;
            case '*': opcode = 3; break;
            case '/': opcode = 4; break;
            default:
                printf("Invalid operator\n");
                break;
        }
        
        if (setarith(&sheet->cells[target_row][target_col], val1, val2, opcode) != 0) {
            printf("Arithmetic error (possibly division by zero)\n");
        }
        display(sheet);
                break;
            }   
           case CMD_INVALID:
               printf("Invalid command\n");
               break;

                case CMD_SETFUNC: {
                    // Parse target cell
                    int target_row = 0, target_col = 0;
                    int i = 0;
                    while(cmd->args[0][i] >= 'A' && cmd->args[0][i] <= 'Z') {
                        target_col = target_col * 26 + (cmd->args[0][i] - 'A' + 1);
                        i++;
                    }
                    target_col--; // Convert to 0-based indexing
                    target_row = atoi(cmd->args[0] + i) - 1;
                
                    // Validate target cell
                    if (target_row < 0 || target_row >= sheet->rows || 
                        target_col < 0 || target_col >= sheet->cols) {
                        printf("Invalid target cell reference\n");
                        break;
                    }
                
                    int opcode = atoi(cmd->args[2]);
                
                    // Special handling for SLEEP
                    if (opcode == 6) {  
                        int sleep_time = atoi(cmd->args[1]); // Get the sleep duration
                        
                        if (cmd->args[1][0] >= 'A' && cmd->args[1][0] <= 'Z') {
                            int row = 0, col = 0;
                            i = 0;
                            while(cmd->args[1][i] >= 'A' && cmd->args[1][i] <= 'Z') {
                                col = col * 26 + (cmd->args[1][i] - 'A' + 1);
                                i++;
                            }
                            col--;
                            row = atoi(cmd->args[1] + i) - 1;

                            if (row < 0 || row >= sheet->rows || col < 0 || col >= sheet->cols) {
                                printf("Invalid cell reference for SLEEP argument\n");
                                break;
                            }
                            
                            if (sheet->cells[row][col].has_error) {

                                sheet->cells[target_row][target_col].has_error = 1;
                                display(sheet);
                                break;
                            }

                            sleep_time = sheet->cells[row][col].value;
                        }

                        if (sleep_time <= 0) {  
                            printf("SLEEP time must be a positive integer!\n");
                            break;
                        }
                
                        sleep(sleep_time);
                        sheet->cells[target_row][target_col].value = sleep_time;
                        sheet->cells[target_row][target_col].has_error = 0;
                        display(sheet);
                        break;
                    }
                
                    // Handle other range-based functions
                    char* range_str = cmd->args[1];
                    char* colon = strchr(range_str, ':');
                    int start_row = 0, start_col = 0, end_row = 0, end_col = 0;
                
                    // Parse start cell
                    i = 0;
                    while(range_str[i] >= 'A' && range_str[i] <= 'Z') {
                        start_col = start_col * 26 + (range_str[i] - 'A' + 1);
                        i++;
                    }
                    start_col--; // Convert to 0-based indexing
                    start_row = atoi(range_str + i) - 1;
                
                    // If there's a range (colon found), parse end cell
                    if (colon) {
                        char* end_cell = colon + 1;
                        i = 0;
                        while(end_cell[i] >= 'A' && end_cell[i] <= 'Z') {
                            end_col = end_col * 26 + (end_cell[i] - 'A' + 1);
                            i++;
                        }
                        end_col--; // Convert to 0-based indexing
                        end_row = atoi(end_cell + i) - 1;
                    } else {
                        // Single cell case
                        end_row = start_row;
                        end_col = start_col;
                    }
                
                    // Validate range
                    if (start_row < 0 || start_row >= sheet->rows || 
                        start_col < 0 || start_col >= sheet->cols ||
                        end_row < 0 || end_row >= sheet->rows ||
                        end_col < 0 || end_col >= sheet->cols ||
                        end_row < start_row || end_col < start_col) {
                        printf("Invalid range specification\n");
                        break;
                    }
                
                    // Collect values from range
                    int rows = end_row - start_row + 1;
                    int cols = end_col - start_col + 1;
                    int count = rows * cols;
                    int* values = malloc(count * sizeof(int));
                    if (!values) {
                        printf("Memory allocation failed\n");
                        break;
                    }
                
                    int idx = 0;
                    int range_has_error = 0;
                    for (int r = start_row; r <= end_row; r++) {
                        for (int c = start_col; c <= end_col; c++) {
                            values[idx++] = sheet->cells[r][c].value;
                            if (sheet->cells[r][c].has_error) {
                                range_has_error = 1;
                            }
                        }
                    }
                
                    // Apply function
                    if (setfunc(&sheet->cells[target_row][target_col], values, count, range_has_error, opcode) != 0) {
                        printf("Error applying function to range\n");
                    }
                
                    free(values);
                    update_dependent_cells(sheet, target_row, target_col);
                    display(sheet);
                    break;
                }
                

       }

       // Free command and its arguments
       for(int i = 0; i < 4; i++) {
           free(cmd->args[i]);
       }
       free(cmd);
   }

   // Cleanup code
   if (sheet) {
       if (sheet->cells) {
           for (int i = 0; i < sheet->rows; i++) {
               if (sheet->cells[i]) {
                   for (int j = 0; j < sheet->cols; j++) {
                       free(sheet->cells[i][j].formula);

                       DependencyNode* curr = sheet->cells[i][j].depends_on;
                       while (curr) {
                        DependencyNode* temp = curr;
                        curr = curr->next;
                        free(temp);
                       }

                       curr = sheet->cells[i][j].dependents;
                       while (curr) {
                            DependencyNode* temp = curr;
                            curr = curr->next;
                            free(temp);
                        }
                       
                   }
                   free(sheet->cells[i]);
               }
           }
           free(sheet->cells);
       }
       free(sheet);
   }

   return 0;
}

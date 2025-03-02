#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<math.h>
#include<time.h>

struct Sheet;
struct Cell;

int setarith(struct Cell* target_cell, int val1, int val2, int opcode);
int setfunc(struct Cell* target_cell, int* values, int count, int opcode);
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
    struct DependencyNode* depends_on;    
    struct DependencyNode* dependents;    
    int has_error;                        
};

enum CommandType {
    CMD_CONTROL, 
    CMD_SETCONST,    
    CMD_SETARITH,
    CMD_SETFUNC,  
    CMD_SETRANGE,   
    CMD_DISABLE_OUTPUT, 
    CMD_ENABLE_OUTPUT,  
    CMD_SCROLL_TO,    
    CMD_INVALID     
};

struct Command {
    enum CommandType type;
    char* args[4];  
 };

struct Sheet {
    struct Cell** cells;
    int rows;
    int cols;
    int view_row;  
    int view_col;  
    int suppress_output;  
};

double calculate_stdev(int* values, int count) {
    if (count <= 1) return 0;
    double mean = 0;
    for (int i = 0; i < count; i++) {
        mean += values[i];
    }
    mean /= count;
    double sum_sq_diff = 0;
    for (int i = 0; i < count; i++) {
        double diff = values[i] - mean;
        sum_sq_diff += diff * diff;
    }
    return sqrt(sum_sq_diff / (count - 1));
}
void add_dependency(struct Cell* dependent, int dep_row, int dep_col) {
    DependencyNode* curr = dependent->depends_on;
    while (curr) {
        if (curr->row == dep_row && curr->col == dep_col) {
            return;
        }
        curr = curr->next;
    }
    DependencyNode* new_node = malloc(sizeof(DependencyNode));
    if (!new_node) return;
    
    new_node->row = dep_row;
    new_node->col = dep_col;
    new_node->next = dependent->depends_on;
    dependent->depends_on = new_node;
}
void add_dependent(struct Cell* dependency, int dep_row, int dep_col) {
    DependencyNode* curr = dependency->dependents;
    while (curr) {
        if (curr->row == dep_row && curr->col == dep_col) {
            return; 
        }
        curr = curr->next;
    }
    DependencyNode* new_node = malloc(sizeof(DependencyNode));
    if (!new_node) return;
    
    new_node->row = dep_row;
    new_node->col = dep_col;
    new_node->next = dependency->dependents;
    dependency->dependents = new_node;
}
void clear_dependencies(struct Cell* cell) {
    DependencyNode* curr = cell->depends_on;
    while (curr) {
        DependencyNode* dep_node = curr;
        curr = curr->next;
        free(dep_node);
    }
    cell->depends_on = NULL;
}

//Below fumctiom prints dependencies and was used for debugging
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
int setarith(struct Cell* target_cell, int val1, int val2, int opcode) {
    int result;
    target_cell->has_error = 0; // Reset error flag
    
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
                return 1; 
            }
            result = val1 / val2;
            break;
        default:
            target_cell->has_error = 1;  
            return 1;  
    }
    
    target_cell->value = result;
    return 0;
}

int setfunc(struct Cell* target_cell, int* values, int count, int opcode) {
    if (count <= 0) return 1;  
    
    int result = 0;
    double temp = 0;  
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
            if (count != 1) return 1;  
            sleep(values[0]); 
            result = values[0];
            break;
            
        default:
            return 1; 
    }
    
    target_cell->value = result;
    return 0;
}
int evaluate_cell(struct Sheet* sheet, int row, int col) {
    struct Cell* cell = &sheet->cells[row][col];
    if (!cell->formula) return 0;
    
    cell->has_error = 0;
    
    char* formula = strdup(cell->formula);
    if (!formula) return 1;
    
    int result = 0;
    if (formula[0] >= 'A' && formula[0] <= 'Z' && strchr(formula, '+') == NULL && 
        strchr(formula, '-') == NULL && strchr(formula, '*') == NULL && 
        strchr(formula, '/') == NULL && strchr(formula, '(') == NULL) {
        int source_row = 0, source_col = 0;
        int i = 0;
        while (formula[i] >= 'A' && formula[i] <= 'Z') {
            source_col = source_col * 26 + (formula[i] - 'A' + 1);
            i++;
        }
        source_col--; 
        source_row = atoi(formula + i) - 1;
        if (source_row < 0 || source_row >= sheet->rows || 
            source_col < 0 || source_col >= sheet->cols) {
            free(formula);
            cell->has_error = 1;
            return 1; 
        }
        
        if (sheet->cells[source_row][source_col].has_error) {
            cell->has_error = 1;
        } else {
            cell->value = sheet->cells[source_row][source_col].value;
        }
        free(formula);
        return 0;
    }
    char* op_ptr = NULL;
    if ((op_ptr = strchr(formula, '+')) || 
        (op_ptr = strchr(formula, '-')) || 
        (op_ptr = strchr(formula, '*')) || 
        (op_ptr = strchr(formula, '/'))) {
        
        char op = *op_ptr;
        *op_ptr = '\0'; 
        
        char* left = formula;
        char* right = op_ptr + 1;
        int val1, val2;
        int left_has_error = 0, right_has_error = 0;
        
        if (left[0] >= 'A' && left[0] <= 'Z') {
            int source_row = 0, source_col = 0;
            int i = 0;
            while (left[i] >= 'A' && left[i] <= 'Z') {
                source_col = source_col * 26 + (left[i] - 'A' + 1);
                i++;
            }
            source_col--; 
            source_row = atoi(left + i) - 1; 
            if (source_row < 0 || source_row >= sheet->rows || 
                source_col < 0 || source_col >= sheet->cols) {
                free(formula);
                cell->has_error = 1;
                return 1;
            }
            
            if (sheet->cells[source_row][source_col].has_error) {
                left_has_error = 1;
            } else {
                val1 = sheet->cells[source_row][source_col].value;
            }
        } else {
            val1 = atoi(left);
        }
        
        if (right[0] >= 'A' && right[0] <= 'Z') {
            int source_row = 0, source_col = 0;
            int i = 0;
            while (right[i] >= 'A' && right[i] <= 'Z') {
                source_col = source_col * 26 + (right[i] - 'A' + 1);
                i++;
            }
            source_col--; 
            source_row = atoi(right + i) - 1; 
            if (source_row < 0 || source_row >= sheet->rows || 
                source_col < 0 || source_col >= sheet->cols) {
                free(formula);
                cell->has_error = 1;
                return 1;
            }
            
            if (sheet->cells[source_row][source_col].has_error) {
                right_has_error = 1;
            } else {
                val2 = sheet->cells[source_row][source_col].value;
            }
        } else {
            val2 = atoi(right);
        }
        
        if (left_has_error || right_has_error) {
            cell->has_error = 1;
            free(formula);
            return 0;
        }
        
        int opcode;
        switch (op) {
            case '+': opcode = 1; break;
            case '-': opcode = 2; break;
            case '*': opcode = 3; break;
            case '/': opcode = 4; break;
            default:
                free(formula);
                cell->has_error = 1;
                return 1;
        }
        
        if (setarith(cell, val1, val2, opcode) != 0) {
            free(formula);
            return 1;
        }
        
        free(formula);
        return 0;
    }
    char* open_paren = strchr(formula, '(');
    char* close_paren = strrchr(formula, ')');
    
    if (open_paren && close_paren) {
        int func_len = open_paren - formula;
        char func_name[16] = {0};
        strncpy(func_name, formula, func_len);
        *close_paren = '\0'; 
        char* args = open_paren + 1;
        if (strcmp(func_name, "SLEEP") == 0) {
            int sleep_time;
            
            // Check if args is a cell reference
            if (args[0] >= 'A' && args[0] <= 'Z') {
                int source_row = 0, source_col = 0;
                int j = 0;
                while (args[j] >= 'A' && args[j] <= 'Z') {
                    source_col = source_col * 26 + (args[j] - 'A' + 1);
                    j++;
                }
                source_col--;
                source_row = atoi(args + j) - 1;
                
                if (source_row < 0 || source_row >= sheet->rows || 
                    source_col < 0 || source_col >= sheet->cols) {
                    free(formula);
                    cell->has_error = 1;
                    return 1;
                }
                
                if (sheet->cells[source_row][source_col].has_error) {
                    cell->has_error = 1;
                    free(formula);
                    return 0;
                }
                
                sleep_time = sheet->cells[source_row][source_col].value;
                
                // Add dependency relationship
                add_dependency(cell, source_row, source_col);
                add_dependent(&sheet->cells[source_row][source_col], row, col);
            } else {
                sleep_time = atoi(args);
            }
            
            if (sleep_time <= 0) {
                free(formula);
                cell->has_error = 1;
                return 1;
            }
            
            sleep(sleep_time);
            cell->value = sleep_time;
            free(formula);
            return 0;
        }
        int opcode;
        if (strcmp(func_name, "MIN") == 0) opcode = 1;
        else if (strcmp(func_name, "MAX") == 0) opcode = 2;
        else if (strcmp(func_name, "AVG") == 0) opcode = 3;
        else if (strcmp(func_name, "SUM") == 0) opcode = 4;
        else if (strcmp(func_name, "STDEV") == 0) opcode = 5;
        else {
            free(formula);
            cell->has_error = 1;
            return 1; 
        }
        char* colon = strchr(args, ':');
        int start_row, start_col, end_row, end_col;
        int i = 0;
        start_col = 0;
        while (args[i] >= 'A' && args[i] <= 'Z') {
            start_col = start_col * 26 + (args[i] - 'A' + 1);
            i++;
        }
        start_col--; 
        start_row = atoi(args + i) - 1; 
        if (colon) {
            char* end_cell = colon + 1;
            i = 0;
            end_col = 0;
            while (end_cell[i] >= 'A' && end_cell[i] <= 'Z') {
                end_col = end_col * 26 + (end_cell[i] - 'A' + 1);
                i++;
            }
            end_col--;
            end_row = atoi(end_cell + i) - 1; 
        } else {
            end_row = start_row;
            end_col = start_col;
        }
        if (start_row < 0 || start_row >= sheet->rows || 
            start_col < 0 || start_col >= sheet->cols ||
            end_row < 0 || end_row >= sheet->rows ||
            end_col < 0 || end_col >= sheet->cols ||
            end_row < start_row || end_col < start_col) {
            free(formula);
            cell->has_error = 1;
            return 1; 
        }
        
        int has_error = 0;
        for (int r = start_row; r <= end_row; r++) {
            for (int c = start_col; c <= end_col; c++) {
                if (sheet->cells[r][c].has_error) {
                    has_error = 1;
                    break;
                }
            }
            if (has_error) break;
        }
        
        if (has_error) {
            cell->has_error = 1;
            free(formula);
            return 0;
        }
        
        int rows = end_row - start_row + 1;
        int cols = end_col - start_col + 1;
        int count = rows * cols;
        int* values = malloc(count * sizeof(int));
        if (!values) {
            free(formula);
            cell->has_error = 1;
            return 1; 
        }
        
        int idx = 0;
        for (int r = start_row; r <= end_row; r++) {
            for (int c = start_col; c <= end_col; c++) {
                values[idx++] = sheet->cells[r][c].value;
            }
        }
        if (setfunc(cell, values, count, opcode) != 0) {
            free(values);
            free(formula);
            cell->has_error = 1;
            return 1; 
        }
        
        free(values);
        free(formula);
        return 0;
    }
    
    free(formula);
    cell->has_error = 1;
    return 1; 
}

int detect_cycle(struct Sheet* sheet, int row, int col, int* visited, int* in_stack) {
    int cell_idx = row * sheet->cols + col;

    if (visited[cell_idx]) return 0;

    visited[cell_idx] = 1;
    in_stack[cell_idx] = 1;
    DependencyNode* curr = sheet->cells[row][col].depends_on;
    while (curr) {
        int dep_idx = curr->row * sheet->cols + curr->col;
        if (in_stack[dep_idx]) return 1;
        if (!visited[dep_idx]) {
            if (detect_cycle(sheet, curr->row, curr->col, visited, in_stack)) {
                return 1;
            }
        }
        
        curr = curr->next;
    }
    in_stack[cell_idx] = 0;
    return 0;
}
void topological_sort_visit(struct Sheet* sheet, int row, int col, int* visited, int** order, int* order_idx) {
    int cell_idx = row * sheet->cols + col;
    if (visited[cell_idx]) return;
    
    visited[cell_idx] = 1;
    DependencyNode* curr = sheet->cells[row][col].dependents;
    while (curr) {
        if (!visited[curr->row * sheet->cols + curr->col]) {
            topological_sort_visit(sheet, curr->row, curr->col, visited, order, order_idx);
        }
        curr = curr->next;
    }
    (*order)[*order_idx * 2] = row;
    (*order)[*order_idx * 2 + 1] = col;
    (*order_idx)++;
}
int update_dependencies(struct Sheet* sheet, int row, int col) {
    int total_cells = sheet->rows * sheet->cols;
    int* visited = calloc(total_cells, sizeof(int));
    int* eval_order = malloc(total_cells * 2 * sizeof(int));
    if (!visited || !eval_order) {
        free(visited);
        free(eval_order);
        return 1; 
    }
    
    int eval_count = 0;
    
    void collect_dependents(int r, int c) {
        int cell_idx = r * sheet->cols + c;
        if (visited[cell_idx]) return;
        
        visited[cell_idx] = 1;
        
        eval_order[eval_count * 2] = r;
        eval_order[eval_count * 2 + 1] = c;
        eval_count++;
        
        DependencyNode* dep = sheet->cells[r][c].dependents;
        while (dep) {
            collect_dependents(dep->row, dep->col);
            dep = dep->next;
        }
    }
    
    collect_dependents(row, col);
    
    for (int i = 0; i < eval_count; i++) {
        int r = eval_order[i * 2];
        int c = eval_order[i * 2 + 1];
        
        if (r == row && c == col) continue;
        
        int has_error_dependency = 0;
        DependencyNode* dep = sheet->cells[r][c].depends_on;
        while (dep) {
            if (sheet->cells[dep->row][dep->col].has_error) {
                has_error_dependency = 1;
                break;
            }
            dep = dep->next;
        }
        
        if (has_error_dependency) {
            sheet->cells[r][c].has_error = 1;
        } else {
            evaluate_cell(sheet, r, c);
        }
    }
    
    free(visited);
    free(eval_order);
    return 0;
}
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


void sleep(int seconds) {
    clock_t end_time = clock() + seconds * CLOCKS_PER_SEC;
    while (clock() < end_time) {
        // Busy wait
    }
}

// Helper function to calculate standard deviation


int handle_setfunc(struct Sheet* sheet, char* target_cell_ref, char* range_str, int opcode) {
    int target_row = 0, target_col = 0;

    int i = 0;
    while(target_cell_ref[i] >= 'A' && target_cell_ref[i] <= 'Z') {
        target_col = target_col * 26 + (target_cell_ref[i] - 'A' + 1);
        i++;
    }
    target_col--; // Convert to 0-based indexing
    target_row = atoi(target_cell_ref + i) - 1; // -1 for 0-based indexing
    
   
    if(target_row < 0 || target_row >= sheet->rows || 
       target_col < 0 || target_col >= sheet->cols) {
        return 1; 
    }
    
   
    sheet->cells[target_row][target_col].has_error = 0;

    clear_dependencies(&sheet->cells[target_row][target_col]);

    if (opcode == 6) {  // SLEEP
        int sleep_time;
        
        // Check if range_str is a cell reference
        if (range_str[0] >= 'A' && range_str[0] <= 'Z') {
            int source_row = 0, source_col = 0;
            int j = 0;
            while (range_str[j] >= 'A' && range_str[j] <= 'Z') {
                source_col = source_col * 26 + (range_str[j] - 'A' + 1);
                j++;
            }
            source_col--;
            source_row = atoi(range_str + j) - 1;
            
            if (source_row < 0 || source_row >= sheet->rows || 
                source_col < 0 || source_col >= sheet->cols) {
                sheet->cells[target_row][target_col].has_error = 1;
                return 1;
            }
            
            if (sheet->cells[source_row][source_col].has_error) {
                sheet->cells[target_row][target_col].has_error = 1;
                return 0;
            }
            
            sleep_time = sheet->cells[source_row][source_col].value;
            
            // Add dependency relationship
            add_dependency(&sheet->cells[target_row][target_col], source_row, source_col);
            add_dependent(&sheet->cells[source_row][source_col], target_row, target_col);
        } else {
            sleep_time = atoi(range_str);
        }
        
        if (sleep_time <= 0) {
            sheet->cells[target_row][target_col].has_error = 1;
            return 1;
        }
        
        char formula[256];
        sprintf(formula, "SLEEP(%s)", range_str);
        free(sheet->cells[target_row][target_col].formula);
        sheet->cells[target_row][target_col].formula = strdup(formula);
        
        sleep(sleep_time);
        sheet->cells[target_row][target_col].value = sleep_time;
        
        return 0;
    }

    char* colon = strchr(range_str, ':');
    int start_row = 0, start_col = 0, end_row = 0, end_col = 0;

    i = 0;
    while(range_str[i] >= 'A' && range_str[i] <= 'Z') {
        start_col = start_col * 26 + (range_str[i] - 'A' + 1);
        i++;
    }
    start_col--; 
    start_row = atoi(range_str + i) - 1;

    if (colon) {
        char* end_cell = colon + 1;
        i = 0;
        while(end_cell[i] >= 'A' && end_cell[i] <= 'Z') {
            end_col = end_col * 26 + (end_cell[i] - 'A' + 1);
            i++;
        }
        end_col--; 
        end_row = atoi(end_cell + i) - 1; 
    } else {

        end_row = start_row;
        end_col = start_col;
    }

    if (start_row < 0 || start_row >= sheet->rows || 
        start_col < 0 || start_col >= sheet->cols ||
        end_row < 0 || end_row >= sheet->rows ||
        end_col < 0 || end_col >= sheet->cols ||
        end_row < start_row || end_col < start_col) {
        sheet->cells[target_row][target_col].has_error = 1;
        return 1; 
    }

    char func_name[10];
    switch (opcode) {
        case 1: strcpy(func_name, "MIN"); break;
        case 2: strcpy(func_name, "MAX"); break;
        case 3: strcpy(func_name, "AVG"); break;
        case 4: strcpy(func_name, "SUM"); break;
        case 5: strcpy(func_name, "STDEV"); break;
        default: 
            sheet->cells[target_row][target_col].has_error = 1;
            return 1; 
    }
    
    char formula[256];
    sprintf(formula, "%s(%s)", func_name, range_str);
    free(sheet->cells[target_row][target_col].formula);
    sheet->cells[target_row][target_col].formula = strdup(formula);

    int has_error_in_range = 0;

    for (int r = start_row; r <= end_row; r++) {
        for (int c = start_col; c <= end_col; c++) {

            if (sheet->cells[r][c].has_error) {
                has_error_in_range = 1;
            }
            
            add_dependency(&sheet->cells[target_row][target_col], r, c);
            add_dependent(&sheet->cells[r][c], target_row, target_col);
        }
    }
    

    if (has_error_in_range) {
        sheet->cells[target_row][target_col].has_error = 1;
        update_dependencies(sheet, target_row, target_col);
        return 0;
    }

    int rows = end_row - start_row + 1;
    int cols = end_col - start_col + 1;
    int count = rows * cols;
    int* values = malloc(count * sizeof(int));
    if (!values) {
        sheet->cells[target_row][target_col].has_error = 1;
        return 1; 
    }
    
    int idx = 0;
    for (int r = start_row; r <= end_row; r++) {
        for (int c = start_col; c <= end_col; c++) {
            values[idx++] = sheet->cells[r][c].value;
        }
    }
    

    if (setfunc(&sheet->cells[target_row][target_col], values, count, opcode) != 0) {
        free(values);
        sheet->cells[target_row][target_col].has_error = 1;
        update_dependencies(sheet, target_row, target_col);
        return 1; 
    }
    
    free(values);
    

    update_dependencies(sheet, target_row, target_col);
    
    return 0;
}
int setconst(char* cell_ref, char* value, struct Sheet* sheet) {
    int target_row = 0, target_col = 0;
    
    int i = 0;
    while(cell_ref[i] >= 'A' && cell_ref[i] <= 'Z') {
        target_col = target_col * 26 + (cell_ref[i] - 'A' + 1);
        i++;
    }
    target_col--;
    target_row = atoi(cell_ref + i) - 1; 
    
    if(target_row < 0 || target_row >= sheet->rows || 
       target_col < 0 || target_col >= sheet->cols) {
        return 1; 
    }
    
    sheet->cells[target_row][target_col].has_error = 0;
    
    clear_dependencies(&sheet->cells[target_row][target_col]);
    
    int val;
    
    if(value[0] >= 'A' && value[0] <= 'Z') {
        int source_row = 0, source_col = 0;
        i = 0;
        while(value[i] >= 'A' && value[i] <= 'Z') {
            source_col = source_col * 26 + (value[i] - 'A' + 1);
            i++;
        }
        source_col--;
        source_row = atoi(value + i) - 1; 
        
        if(source_row < 0 || source_row >= sheet->rows || 
           source_col < 0 || source_col >= sheet->cols) {
            sheet->cells[target_row][target_col].has_error = 1;
            return 1; 
        }
        
        if (sheet->cells[source_row][source_col].has_error) {
            sheet->cells[target_row][target_col].has_error = 1;
        } else {
            val = sheet->cells[source_row][source_col].value;
            sheet->cells[target_row][target_col].value = val;
        }
        
        add_dependency(&sheet->cells[target_row][target_col], source_row, source_col);
        add_dependent(&sheet->cells[source_row][source_col], target_row, target_col);
        
        free(sheet->cells[target_row][target_col].formula);
        sheet->cells[target_row][target_col].formula = strdup(value);
    } else {
        val = atoi(value);
        sheet->cells[target_row][target_col].value = val;
        
        free(sheet->cells[target_row][target_col].formula);
        sheet->cells[target_row][target_col].formula = NULL;
    }
    
    update_dependencies(sheet, target_row, target_col);
    return 0;
}
void get_column_name(int col, char* buffer) {
    // 0-based indexing
    col--;
    
    if (col < 26) {  
        buffer[0] = 'A' + col;
        buffer[1] = '\0';
        return;
    }
    
    // columns after Z
    int first = col / 26;
    int second = col % 26;
    
    if (first <= 26) {  
        buffer[0] = 'A' + first - 1;
        buffer[1] = 'A' + second;
        buffer[2] = '\0';
    } else {  
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
 
    if (sheet->suppress_output) {
        return 0;
    }
    
    char col_name[4];
    

    int end_row = sheet->view_row + 10;
    int end_col = sheet->view_col + 10;
    if (end_row > sheet->rows) end_row = sheet->rows;
    if (end_col > sheet->cols) end_col = sheet->cols;
    

    printf("    ");
    for(int j = sheet->view_col; j < end_col; j++) {
        get_column_name(j + 1, col_name);
        printf("%-4s ", col_name);
    }
    printf("\n");
    

    for(int i = sheet->view_row; i < end_row; i++) {
        printf("%-3d ", i + 1);
        for(int j = sheet->view_col; j < end_col; j++) {
            if (sheet->cells[i][j].has_error) {
                printf("ERR  ");
            } else {
                printf("%-4d ", sheet->cells[i][j].value);
            }
        }
        printf("\n");
    }
    return 0;
}
int scroll_to(struct Sheet* sheet, char* cell_ref) {
    int target_row = 0, target_col = 0;
    
    int i = 0;
    while (cell_ref[i] >= 'A' && cell_ref[i] <= 'Z') {
        target_col = target_col * 26 + (cell_ref[i] - 'A' + 1);
        i++;
    }
    target_col--; 
    target_row = atoi(cell_ref + i) - 1;

    if (target_row < 0 || target_row >= sheet->rows || 
        target_col < 0 || target_col >= sheet->cols) {
        return 1; 
    }

    sheet->view_row = target_row;
    sheet->view_col = target_col;
    
    return 0;
}

int control(char* input, struct Sheet* sheet) {
    switch (input[0]) {
        case 'w':  
            if (sheet->view_row >= 10) {
                sheet->view_row -= 10;  
            } else {
                sheet->view_row = 0;  
            }
            break;

        case 's': 
            if (sheet->view_row + 20 <= sheet->rows) {
                sheet->view_row += 10;  
            } else if (sheet->rows > 10) {
                sheet->view_row = sheet->rows - 10;  
            }
            break;

        case 'a':  
            if (sheet->view_col >= 10) {
                sheet->view_col -= 10;  
            } else {
                sheet->view_col = 0;  
            }
            break;

        case 'd':
            if (sheet->view_col + 20 <= sheet->cols) {
                sheet->view_col += 10; 
            } else if (sheet->cols > 10) {
                sheet->view_col = sheet->cols - 10;  
            }
            break;

        default: 
            return 1;
    }
    return 0; 
}
int handle_setarith(struct Sheet* sheet, char* target_cell_ref, char* left_operand, char* right_operand, char op) {
    int target_row = 0, target_col = 0;

    int i = 0;
    while(target_cell_ref[i] >= 'A' && target_cell_ref[i] <= 'Z') {
        target_col = target_col * 26 + (target_cell_ref[i] - 'A' + 1);
        i++;
    }
    target_col--; 
    target_row = atoi(target_cell_ref + i) - 1; 

    if(target_row < 0 || target_row >= sheet->rows || 
       target_col < 0 || target_col >= sheet->cols) {
        return 1; 
    }
    
    sheet->cells[target_row][target_col].has_error = 0;
    
    clear_dependencies(&sheet->cells[target_row][target_col]);

    int val1, val2;
    int left_row = -1, left_col = 0;  
    int right_row = -1, right_col = 0;  
    int left_has_error = 0, right_has_error = 0;
    
    if(left_operand[0] >= 'A' && left_operand[0] <= 'Z') {
        i = 0;
        while(left_operand[i] >= 'A' && left_operand[i] <= 'Z') {
            left_col = left_col * 26 + (left_operand[i] - 'A' + 1);
            i++;
        }
        left_col--;
        left_row = atoi(left_operand + i) - 1;
        if(left_row < 0 || left_row >= sheet->rows || left_col < 0 || left_col >= sheet->cols) {
            sheet->cells[target_row][target_col].has_error = 1;
            return 1;
        }
        
        if (sheet->cells[left_row][left_col].has_error) {
            left_has_error = 1;
        } else {
            val1 = sheet->cells[left_row][left_col].value;
        }
        
        add_dependency(&sheet->cells[target_row][target_col], left_row, left_col);
        add_dependent(&sheet->cells[left_row][left_col], target_row, target_col);
    } else {
        val1 = atoi(left_operand);
    }
    
    if(right_operand[0] >= 'A' && right_operand[0] <= 'Z') {
        i = 0;
        while(right_operand[i] >= 'A' && right_operand[i] <= 'Z') {
            right_col = right_col * 26 + (right_operand[i] - 'A' + 1);
            i++;
        }
        right_col--;
        right_row = atoi(right_operand + i) - 1;
        if(right_row < 0 || right_row >= sheet->rows || right_col < 0 || right_col >= sheet->cols) {
            sheet->cells[target_row][target_col].has_error = 1;
            return 1; 
        }
        
        if (sheet->cells[right_row][right_col].has_error) {
            right_has_error = 1;
        } else {
            val2 = sheet->cells[right_row][right_col].value;
        }
        
        add_dependency(&sheet->cells[target_row][target_col], right_row, right_col);
        add_dependent(&sheet->cells[right_row][right_col], target_row, target_col);
    } else {
        val2 = atoi(right_operand);
    }
    
    char formula[256];
    sprintf(formula, "%s%c%s", left_operand, op, right_operand);
    free(sheet->cells[target_row][target_col].formula);
    sheet->cells[target_row][target_col].formula = strdup(formula);
    
    if (left_has_error || right_has_error) {
        sheet->cells[target_row][target_col].has_error = 1;
        update_dependencies(sheet, target_row, target_col);
        return 0;
    }
    
    int opcode;
    switch(op) {
        case '+': opcode = 1; break;
        case '-': opcode = 2; break;
        case '*': opcode = 3; break;
        case '/': opcode = 4; break;
        default: 
            sheet->cells[target_row][target_col].has_error = 1;
            update_dependencies(sheet, target_row, target_col);
            return 1; 
    }
    
    if (opcode == 4 && val2 == 0) {
        sheet->cells[target_row][target_col].has_error = 1;
        update_dependencies(sheet, target_row, target_col);
        return 1;
    }
    
    if (setarith(&sheet->cells[target_row][target_col], val1, val2, opcode) != 0) {
        update_dependencies(sheet, target_row, target_col);
        return 1; 
    }
    
    update_dependencies(sheet, target_row, target_col);
    return 0;
}
struct Command* parse_input(char* input) {
    struct Command* cmd = malloc(sizeof(struct Command));
    if (cmd == NULL) return NULL;
    
    for(int i = 0; i < 4; i++) {
        cmd->args[i] = NULL;
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
        cmd->args[0] = strdup(input + 10); 
        return cmd;
    }
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
    char* equals = strchr(input, '=');
    if (equals != NULL) {
        *equals = '\0';  
        char* cell = input;
        char* value = equals + 1;
        while(*cell && isspace(*cell)) cell++;
        while(*value && isspace(*value)) value++;
        char* end = cell + strlen(cell) - 1;
        while(end > cell && isspace(*end)) *end-- = '\0';
        end = value + strlen(value) - 1;
        while(end > value && isspace(*end)) *end-- = '\0';
        char* open_paren = strchr(value, '(');
        char* close_paren = strchr(value, ')');
        if (open_paren && close_paren && open_paren < close_paren) {
            int func_len = open_paren - value;
            char func_name[10] = {0};  
            strncpy(func_name, value, func_len);
            char* arg = open_paren + 1;
            *close_paren = '\0';
            
            cmd->type = CMD_SETFUNC;
            cmd->args[0] = strdup(cell);     
            
            if (strcmp(func_name, "SLEEP") == 0) {
                cmd->args[1] = strdup(arg);
                cmd->args[2] = strdup("6");
                return cmd;
            }

            cmd->args[1] = strdup(arg);


            if (strcmp(func_name, "MIN") == 0) cmd->args[2] = strdup("1");
            else if (strcmp(func_name, "MAX") == 0) cmd->args[2] = strdup("2");
            else if (strcmp(func_name, "AVG") == 0) cmd->args[2] = strdup("3");
            else if (strcmp(func_name, "SUM") == 0) cmd->args[2] = strdup("4");
            else if (strcmp(func_name, "STDEV") == 0) cmd->args[2] = strdup("5");
            else {
                cmd->type = CMD_INVALID;
                return cmd;
            }
            return cmd;
        }
        char* operator = strpbrk(value, "+-*/");
        if (operator != NULL) {

            char opcode = *operator;
            *operator = '\0';  
            
            cmd->type = CMD_SETARITH;
            cmd->args[0] = malloc(strlen(cell) + 1);
            cmd->args[1] = malloc(strlen(value) + 1);
            cmd->args[2] = malloc(strlen(operator+1)+1);
            cmd->args[3] = malloc(2); 
            
            if(cmd->args[0] && cmd->args[1] && cmd->args[2] && cmd->args[3]) {
                strcpy(cmd->args[0], cell);    
                strcpy(cmd->args[1], value);   
                strcpy(cmd->args[2], operator + 1); 
                

                cmd->args[3][0]= opcode;
                cmd->args[3][1] = '\0';
                return cmd;
            } else {
                printf("Memory allocation failed\n");
                free(cmd->args[0]);
                free(cmd->args[1]);
                free(cmd->args[2]);
                free(cmd->args[3]);
                cmd->args[0] = cmd->args[1] = cmd->args[2]= cmd->args[3] = NULL;
                cmd->type = CMD_INVALID;
                *operator = opcode;
                
                return cmd;
            }
        } else if (strlen(value) > 0) {
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
int main(int argc, char *argv[]) {
    struct Sheet* sheet = NULL;
    int state = 0;  // Will store this in sheet later
    char input[1024];
 
    if (state == 0) {
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
        sheet->suppress_output = 0; 
        sheet->cells = malloc(sheet->rows * sizeof(struct Cell*));
        if (sheet->cells == NULL) {
            free(sheet);
            printf("Memory allocation failed\n");
            return 1;
        }
        for (int i = 0; i < sheet->rows; i++) {
            sheet->cells[i] = malloc(sheet->cols * sizeof(struct Cell));
            if (sheet->cells[i] == NULL) {
                for (int j = 0; j < i; j++) {
                    free(sheet->cells[j]);
                }
                free(sheet->cells);
                free(sheet);
                printf("Memory allocation failed\n");
                return 1;
            }
            for (int j = 0; j < sheet->cols; j++) {
                sheet->cells[i][j].value = 0;
                sheet->cells[i][j].formula = NULL;
                sheet->cells[i][j].depends_on = NULL;
                sheet->cells[i][j].dependents = NULL;
                sheet->cells[i][j].has_error = 0; 
            }
        }
        
        state = 1;
        display(sheet);  
    }

    while(state == 1) {
        printf("> ");
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        input[strcspn(input, "\n")] = 0;
 
        struct Command* cmd = parse_input(input);
        if (cmd == NULL) {
            printf("Memory allocation failed\n");
            continue;
        }
        switch(cmd->type) {
            case CMD_CONTROL:
                if (cmd->args[0][0] == 'q') {
                    state = 0;  
                }
                else {
                    control(cmd->args[0], sheet);
                    if (!sheet->suppress_output) {
                        display(sheet);
                    }
                }
                break;
 
            case CMD_SETCONST:
                if (setconst(cmd->args[0], cmd->args[1], sheet) != 0) {
                    printf("Invalid cell reference or value\n");
                }
                if (!sheet->suppress_output) {
                    display(sheet);
                }
                break;
                
            case CMD_SETARITH:
                if (handle_setarith(sheet, cmd->args[0], cmd->args[1], cmd->args[2], cmd->args[3][0]) != 0) {
                    printf("Arithmetic error (division-by-zero, or invalid reference)\n");
                }
                if (!sheet->suppress_output) {
                    display(sheet);
                }
                break;
               
            case CMD_SETFUNC:
                if (handle_setfunc(sheet, cmd->args[0], cmd->args[1], atoi(cmd->args[2])) != 0) {
                    printf("Error applying function to range\n");
                }
                if (!sheet->suppress_output) {
                    display(sheet);
                }
                break;
                
            case CMD_DISABLE_OUTPUT:
                sheet->suppress_output = 1;
                break;
                
            case CMD_ENABLE_OUTPUT:
                if (sheet->suppress_output) {
                    sheet->suppress_output = 0;
                    
                }
                break;
                
            case CMD_SCROLL_TO:
                if (scroll_to(sheet, cmd->args[0]) != 0) {
                    printf("Invalid cell reference for scroll_to\n");
                }
                if (!sheet->suppress_output) {
                    display(sheet);
                }
                break;
                
            case CMD_INVALID:
                printf("Invalid command\n");
                break;
        }
        for(int i = 0; i < 4; i++) {
            free(cmd->args[i]);
        }
        free(cmd);
    }
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

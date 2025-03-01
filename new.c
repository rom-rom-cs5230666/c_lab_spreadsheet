#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<math.h>
#include<time.h>
struct Cell {
    int value;
    char* formula;
};

enum CommandType {
   CMD_CONTROL,     // w,a,s,d,q commands
   CMD_SETCONST,    // direct value assignment
   CMD_SETARITH,    // arithmetic operations
   CMD_SETFUNC,     // functions like MIN, MAX etc
   CMD_SETRANGE,    // range operations
   CMD_INVALID      // for error handling
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
   // No need for separate left,right,up,down as view_row and view_col define the window
};


// Helper function to calculate standard deviation
// Custom square root implementation using Newton's method
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
int setfunc(struct Cell* target_cell, int* values, int count, int opcode) {
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
    
    int val;
    
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
    } else {
        // Treat as integer constant
        val = atoi(value);
    }
    
    // Set the value
    sheet->cells[target_row][target_col].value = val;
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
           printf("%-4d ", sheet->cells[i][j].value);
       }
       printf("\n");
   }
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
                return 1;  // Error: division by zero
            }
            result = val1 / val2;
            break;
        default:
            return 1;  // Error: invalid operation
    }
    
    target_cell->value = result;
    free(target_cell->formula);
    target_cell->formula = NULL;
    
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
            char* range = open_paren + 1;
            *close_paren = '\0';
 
            cmd->type = CMD_SETFUNC;
            cmd->args[0] = strdup(cell);     // Target cell
            cmd->args[1] = strdup(range);    // Range
            
            // Determine function type
            if (strcmp(func_name, "MIN") == 0) cmd->args[2] = strdup("1");
            else if (strcmp(func_name, "MAX") == 0) cmd->args[2] = strdup("2");
            else if (strcmp(func_name, "AVG") == 0) cmd->args[2] = strdup("3");
            else if (strcmp(func_name, "SUM") == 0) cmd->args[2] = strdup("4");
            else if (strcmp(func_name, "STDEV") == 0) cmd->args[2] = strdup("5");
            else if (strcmp(func_name, "SLEEP") == 0) cmd->args[2] = strdup("6");
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
                    printf("%d",val1);
                } else {
                    val1 = atoi(cmd->args[1]);

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
                    printf("%d",val2);
                } else {
                    val2 = atoi(cmd->args[2]);
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
                if(target_row < 0 || target_row >= sheet->rows || 
                   target_col < 0 || target_col >= sheet->cols) {
                    printf("Invalid target cell reference\n");
                    break;
                }
                
                // Parse range and collect values
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
                for (int r = start_row; r <= end_row; r++) {
                    for (int c = start_col; c <= end_col; c++) {
                        values[idx++] = sheet->cells[r][c].value;
                    }
                }
                
                // Apply function
                int opcode = atoi(cmd->args[2]);
                if (setfunc(&sheet->cells[target_row][target_col], values, count, opcode) != 0) {
                    printf("Error applying function to range\n");
                }
                
                free(values);
                display(sheet);
                break;
            }   
           // Add other cases as we implement them
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

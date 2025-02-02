#include<stdio.h>
#include<stdlib.h>
#include<string.h>
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
   char* args[3];   // Dynamic array to store arguments
};
struct Sheet {
   struct Cell** cells;
   int rows;
   int cols;
   int view_row;    // Top row of visible window
   int view_col;    // Leftmost column of visible window
   // No need for separate left,right,up,down as view_row and view_col define the window
};


int setconst(char* cell_ref, char* value, struct Sheet* sheet) {
   // Convert value string to integer
   int val = atoi(value);
   
   // Get row and column from cell reference (like "A1", "B2" etc)
   int row = 0, col = 0;
   
   // Skip letters to get row number
   int i = 0;
   while(cell_ref[i] >= 'A' && cell_ref[i] <= 'Z') {
       col = col * 26 + (cell_ref[i] - 'A' + 1);
       i++;
   }
   col--; // Convert to 0-based indexing
   
   // Convert remaining digits to row number
   row = atoi(cell_ref + i) - 1; // -1 for 0-based indexing
   
   // Validate row and column
   if(row < 0 || row >= sheet->rows || col < 0 || col >= sheet->cols) {
       return 1; // Error: invalid cell reference
   }
   
   // Set the value
   sheet->cells[row][col].value = val;
   // Clear any existing formula
   free(sheet->cells[row][col].formula);
   sheet->cells[row][col].formula = NULL;
   
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
   switch(input[0]) {
       case 'w':  // up
           if (sheet->view_row > 0) 
               sheet->view_row -= 10;
           break;
       case 's':  // down
           if (sheet->view_row + 10 < sheet->rows) 
               sheet->view_row += 10;
           break;
       case 'a':  // left
           if (sheet->view_col > 0) 
               sheet->view_col -= 10;
           break;
       case 'd':  // right
           if (sheet->view_col + 10 < sheet->cols) 
               sheet->view_col += 10;
           break;
       default:
           return 1;  // Error for invalid input
   }
   return 0;  // Success
}


struct Command* parse_input(char* input) {
   struct Command* cmd = malloc(sizeof(struct Command));
   if (cmd == NULL) return NULL;
   
   for(int i = 0; i < 3; i++) {
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

   // Check for constant assignment (format: A1=5)
   char* equals = strchr(input, '=');
   if (equals != NULL) {
       *equals = '\0';  // Split at equals sign
       char* cell = input;
       char* value = equals + 1;
       
       // If value contains no operators, it's a constant
       if (strpbrk(value, "+-*/") == NULL) {
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
   }

   // Main program loop
   while(state == 1) {
       display(sheet);
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
       display(sheet);
   }
   break;
            case CMD_SETCONST:
       if (setconst(cmd->args[0], cmd->args[1], sheet) != 0) {
           printf("Invalid cell reference or value\n");
       }
       display(sheet);
       break;


               
           case CMD_INVALID:
               printf("Invalid command\n");
               break;
           // Add other cases as we implement them
       }

       // Free command and its arguments
       for(int i = 0; i < 3; i++) {
           free(cmd->args[i]);
       }
       free(cmd);
   }

   // Cleanup code
   // ... (free all allocated memory)

   return 0;
}
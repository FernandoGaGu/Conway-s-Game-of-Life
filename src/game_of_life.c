/**
 * @file game_of_life.c
 * @brief Conway's Game of Life implementation using SDL2
 * @author Your Name
 * @date 2025
 * 
 * Compilation:
 * 
 * gcc -o gol game_of_life.c -I/opt/homebrew/include -L/opt/homebrew/lib -lSDL2
 * 
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <errno.h>

/* Configuration Constants */
#define CELL_SIZE 8
#define BUFFER_SIZE 2048
#define MAX_CONFIG_LENGTH 200
#define FRAME_DELAY_MS 25
#define NEIGHBORS_TO_BIRTH 3
#define MIN_NEIGHBORS_TO_SURVIVE 2
#define MAX_NEIGHBORS_TO_SURVIVE 3

/* Configuration Keys */
#define CONFIG_RANDOM "random"
#define CONFIG_MANUAL "manual"

/* Cell States */
typedef enum {
    CELL_DEAD = 0,
    CELL_ALIVE = 1,
    CELL_DYING = -1,
    CELL_BIRTH = -2
} cell_state_t;


/* Return Codes */
typedef enum {
    GOL_SUCCESS = 0,
    GOL_ERROR_ARGS = 1,
    GOL_ERROR_FILE = 2,
    GOL_ERROR_CONFIG = 3,
    GOL_ERROR_SDL = 4,
    GOL_ERROR_MEMORY = 5
} gol_result_t;


/* Configuration Structure */
typedef struct {
    size_t rows;
    size_t cols;
    unsigned int steps;
    unsigned int seed;
    char config_type[MAX_CONFIG_LENGTH];
} gol_config_t;

/* Game Context Structure */
typedef struct {
    size_t rows;
    size_t cols;
    cell_state_t **grid;
    SDL_Window *window;
    SDL_Renderer *renderer;
    gol_config_t config;
} gol_context_t;


/* Forward Declarations */
static gol_result_t parse_config_file(const char *filename, gol_config_t *config);
static gol_result_t parse_manual_config(const char *filename, gol_context_t *ctx);
static gol_result_t allocate_grid(gol_context_t *ctx);
static void deallocate_grid(gol_context_t *ctx);
static gol_result_t initialize_sdl(gol_context_t *ctx);
static void cleanup_sdl(gol_context_t *ctx);
static void initialize_grid_random(gol_context_t *ctx);
static void initialize_grid_manual(gol_context_t *ctx, const char *filename);
static void simulate_step(gol_context_t *ctx);
static void render_grid(const gol_context_t *ctx);
static void handle_mouse_click(gol_context_t *ctx, int x, int y);
static int count_neighbors(const gol_context_t *ctx, size_t row, size_t col);
static int count_alive_cells(const gol_context_t *ctx);
static void print_grid_console(const gol_context_t *ctx);
static bool is_valid_position(const gol_context_t *ctx, int row, int col);


/**
 * @brief Parse configuration file
 * @param filename Configuration file path
 * @param config Pointer to configuration structure to fill
 * @return GOL_SUCCESS on success, error code otherwise
 */
static gol_result_t parse_config_file(const char *filename, gol_config_t *config) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open configuration file '%s': %s\n", 
                filename, strerror(errno));
        return GOL_ERROR_FILE;
    }

    char buffer[BUFFER_SIZE];
    bool rows_set = false, cols_set = false, config_set = false;
    
    /* Initialize with default values */
    config->steps = 0;  /* 0 means infinite */
    config->seed = 0;   /* 0 means use time as seed */
    
    while (fgets(buffer, sizeof(buffer), file)) {
        /* Skip empty lines and comments */
        if (buffer[0] == '\n' || buffer[0] == '#') continue;
        
        if (sscanf(buffer, "@nrows %zu", &config->rows) == 1) {
            rows_set = true;
        } else if (sscanf(buffer, "@ncols %zu", &config->cols) == 1) {
            cols_set = true;
        } else if (sscanf(buffer, "@steps %u", &config->steps) == 1) {
            /* Optional parameter */
        } else if (sscanf(buffer, "@seed %u", &config->seed) == 1) {
            /* Optional parameter */
        } else if (sscanf(buffer, "@config %199s", config->config_type) == 1) {
            config_set = true;
        }
    }
    
    fclose(file);
    
    /* Validate required parameters */
    if (!rows_set || !cols_set || !config_set) {
        fprintf(stderr, "Error: Missing required configuration parameters\n");
        return GOL_ERROR_CONFIG;
    }
    
    if (config->rows == 0 || config->cols == 0) {
        fprintf(stderr, "Error: Grid dimensions must be positive\n");
        return GOL_ERROR_CONFIG;
    }
    
    return GOL_SUCCESS;
}

/**
 * @brief Parse manual configuration from file
 * @param filename Configuration file path
 * @param ctx Game context
 * @return GOL_SUCCESS on success, error code otherwise
 */
static gol_result_t parse_manual_config(const char *filename, gol_context_t *ctx) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open configuration file '%s': %s\n", 
                filename, strerror(errno));
        return GOL_ERROR_FILE;
    }

    char buffer[BUFFER_SIZE];
    bool in_grid_section = false;
    size_t current_row = 0;
    
    while (fgets(buffer, sizeof(buffer), file)) {
        /* Skip empty lines and comments */
        if (buffer[0] == '\n' || buffer[0] == '#') continue;
        
        /* Check if we're entering the grid section */
        if (strncmp(buffer, "@grid", 5) == 0) {
            in_grid_section = true;
            current_row = 0;
            continue;
        }
        
        /* Parse grid rows if we're in the grid section */
        if (in_grid_section) {
            /* Skip configuration lines */
            if (buffer[0] == '@') continue;
            
            /* Parse grid row */
            if (current_row < ctx->rows) {
                size_t col = 0;
                for (size_t i = 0; i < strlen(buffer) && col < ctx->cols; i++) {
                    char c = buffer[i];
                    if (c == '1' || c == '#' || c == '*' || c == 'X') {
                        ctx->grid[current_row][col] = CELL_ALIVE;
                        col++;
                    } else if (c == '0' || c == '.' || c == ' ') {
                        ctx->grid[current_row][col] = CELL_DEAD;
                        col++;
                    }
                    /* Skip other characters like separators */
                }
                current_row++;
            }
        }
    }
    
    fclose(file);
    
    if (in_grid_section && current_row < ctx->rows) {
        fprintf(stderr, "Warning: Only %zu of %zu grid rows were specified\n", 
                current_row, ctx->rows);
    }
    
    return GOL_SUCCESS;
}

/**
 * @brief Allocate memory for the game grid
 * @param ctx Game context
 * @return GOL_SUCCESS on success, GOL_ERROR_MEMORY on failure
 */
static gol_result_t allocate_grid(gol_context_t *ctx) {
    /* Allocate array of row pointers */
    ctx->grid = calloc(ctx->rows, sizeof(cell_state_t*));
    if (!ctx->grid) {
        return GOL_ERROR_MEMORY;
    }
    
    /* Allocate memory for each row */
    for (size_t i = 0; i < ctx->rows; i++) {
        ctx->grid[i] = calloc(ctx->cols, sizeof(cell_state_t));
        if (!ctx->grid[i]) {
            /* Clean up previously allocated rows */
            for (size_t j = 0; j < i; j++) {
                free(ctx->grid[j]);
            }
            free(ctx->grid);
            ctx->grid = NULL;
            return GOL_ERROR_MEMORY;
        }
    }
    
    return GOL_SUCCESS;
}

/**
 * @brief Deallocate grid memory
 * @param ctx Game context
 */
static void deallocate_grid(gol_context_t *ctx) {
    if (!ctx->grid) return;
    
    for (size_t i = 0; i < ctx->rows; i++) {
        free(ctx->grid[i]);
    }
    free(ctx->grid);
    ctx->grid = NULL;
}

/**
 * @brief Initialize SDL components
 * @param ctx Game context
 * @return GOL_SUCCESS on success, GOL_ERROR_SDL on failure
 */
static gol_result_t initialize_sdl(gol_context_t *ctx) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Error: SDL initialization failed: %s\n", SDL_GetError());
        return GOL_ERROR_SDL;
    }
    
    int window_width = (int)(ctx->cols * CELL_SIZE);
    int window_height = (int)(ctx->rows * CELL_SIZE);
    
    ctx->window = SDL_CreateWindow(
        "Conway's Game of Life",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        window_width, window_height,
        SDL_WINDOW_SHOWN
    );
    
    if (!ctx->window) {
        fprintf(stderr, "Error: Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return GOL_ERROR_SDL;
    }
    
    ctx->renderer = SDL_CreateRenderer(ctx->window, -1, 
                                      SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ctx->renderer) {
        fprintf(stderr, "Error: Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(ctx->window);
        SDL_Quit();
        return GOL_ERROR_SDL;
    }
    
    return GOL_SUCCESS;
}

/**
 * @brief Clean up SDL resources
 * @param ctx Game context
 */
static void cleanup_sdl(gol_context_t *ctx) {
    if (ctx->renderer) {
        SDL_DestroyRenderer(ctx->renderer);
        ctx->renderer = NULL;
    }
    if (ctx->window) {
        SDL_DestroyWindow(ctx->window);
        ctx->window = NULL;
    }
    SDL_Quit();
}


/**
 * @brief Initialize grid with random values
 * @param ctx Game context
 */
static void initialize_grid_random(gol_context_t *ctx) {
    unsigned int seed = ctx->config.seed;
    if (seed == 0) {
        seed = (unsigned int)time(NULL);
    }
    srand(seed);
    
    for (size_t i = 0; i < ctx->rows; i++) {
        for (size_t j = 0; j < ctx->cols; j++) {
            ctx->grid[i][j] = (rand() % 2) ? CELL_ALIVE : CELL_DEAD;
        }
    }
}

/**
 * @brief Initialize grid with manual configuration
 * @param ctx Game context
 * @param filename Configuration file path
 */
static void initialize_grid_manual(gol_context_t *ctx, const char *filename) {
    /* Initialize all cells to dead first */
    for (size_t i = 0; i < ctx->rows; i++) {
        for (size_t j = 0; j < ctx->cols; j++) {
            ctx->grid[i][j] = CELL_DEAD;
        }
    }
    
    /* Parse and set living cells from configuration */
    parse_manual_config(filename, ctx);
}

/**
 * @brief Check if position is within grid bounds
 * @param ctx Game context
 * @param row Row coordinate
 * @param col Column coordinate
 * @return true if position is valid, false otherwise
 */
static bool is_valid_position(const gol_context_t *ctx, int row, int col) {
    return (row >= 0 && row < (int)ctx->rows && 
            col >= 0 && col < (int)ctx->cols);
}

/**
 * @brief Count living neighbors of a cell
 * @param ctx Game context
 * @param row Cell row
 * @param col Cell column
 * @return Number of living neighbors
 */
static int count_neighbors(const gol_context_t *ctx, size_t row, size_t col) {
    int count = 0;
    
    /* Check all 8 neighboring cells */
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            if (dr == 0 && dc == 0) continue; /* Skip the cell itself */
            
            int nr = (int)row + dr;
            int nc = (int)col + dc;
            
            if (is_valid_position(ctx, nr, nc)) {
                cell_state_t state = ctx->grid[nr][nc];
                if (state == CELL_ALIVE || state == CELL_DYING) {
                    count++;
                }
            }
        }
    }
    
    return count;
}

/**
 * @brief Simulate one generation following Conway's rules
 * @param ctx Game context
 */
static void simulate_step(gol_context_t *ctx) {
    /* Mark cells for state changes */
    for (size_t i = 0; i < ctx->rows; i++) {
        for (size_t j = 0; j < ctx->cols; j++) {
            int neighbors = count_neighbors(ctx, i, j);
            cell_state_t current = ctx->grid[i][j];
            
            if (current == CELL_ALIVE) {
                /* Living cell rules */
                if (neighbors < MIN_NEIGHBORS_TO_SURVIVE || 
                    neighbors > MAX_NEIGHBORS_TO_SURVIVE) {
                    ctx->grid[i][j] = CELL_DYING;
                }
            } else if (current == CELL_DEAD) {
                /* Dead cell rules */
                if (neighbors == NEIGHBORS_TO_BIRTH) {
                    ctx->grid[i][j] = CELL_BIRTH;
                }
            }
        }
    }
    
    /* Apply state changes */
    for (size_t i = 0; i < ctx->rows; i++) {
        for (size_t j = 0; j < ctx->cols; j++) {
            if (ctx->grid[i][j] == CELL_DYING) {
                ctx->grid[i][j] = CELL_DEAD;
            } else if (ctx->grid[i][j] == CELL_BIRTH) {
                ctx->grid[i][j] = CELL_ALIVE;
            }
        }
    }
}

/**
 * @brief Render the grid using SDL
 * @param ctx Game context
 */
static void render_grid(const gol_context_t *ctx) {
    /* Clear screen with black background */
    SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 255);
    SDL_RenderClear(ctx->renderer);
    
    /* Draw living cells in green */
    SDL_SetRenderDrawColor(ctx->renderer, 0, 255, 0, 255);
    
    for (size_t i = 0; i < ctx->rows; i++) {
        for (size_t j = 0; j < ctx->cols; j++) {
            if (ctx->grid[i][j] == CELL_ALIVE) {
                SDL_Rect cell = {
                    .x = (int)(j * CELL_SIZE),
                    .y = (int)(i * CELL_SIZE),
                    .w = CELL_SIZE,
                    .h = CELL_SIZE
                };
                SDL_RenderFillRect(ctx->renderer, &cell);
            }
        }
    }
    
    SDL_RenderPresent(ctx->renderer);
}

/**
 * @brief Handle mouse click to toggle cell state
 * @param ctx Game context
 * @param x Mouse x coordinate
 * @param y Mouse y coordinate
 */
static void handle_mouse_click(gol_context_t *ctx, int x, int y) {
    size_t col = (size_t)(x / CELL_SIZE);
    size_t row = (size_t)(y / CELL_SIZE);
    
    if (row < ctx->rows && col < ctx->cols) {
        /* Toggle cell state */
        ctx->grid[row][col] = (ctx->grid[row][col] == CELL_ALIVE) ? 
                              CELL_DEAD : CELL_ALIVE;
    }
}


/**
 * @brief Count total number of living cells
 * @param ctx Game context
 * @return Number of living cells
 */
static int count_alive_cells(const gol_context_t *ctx) {
    int count = 0;
    for (size_t i = 0; i < ctx->rows; i++) {
        for (size_t j = 0; j < ctx->cols; j++) {
            if (ctx->grid[i][j] == CELL_ALIVE) {
                count++;
            }
        }
    }
    return count;
}

/**
 * @brief Print grid to console for debugging
 * @param ctx Game context
 */
static void print_grid_console(const gol_context_t *ctx) {
    printf("Living cells: %d\n", count_alive_cells(ctx));
    
    for (size_t i = 0; i < ctx->rows; i++) {
        for (size_t j = 0; j < ctx->cols; j++) {
            printf("%c ", (ctx->grid[i][j] == CELL_ALIVE) ? '#' : '.');
        }
        printf("\n");
    }
    printf("\n");
}

/**
 * @brief Main game loop
 * @param ctx Game context
 * @return GOL_SUCCESS on normal exit
 */
static gol_result_t run_simulation(gol_context_t *ctx) {
    bool running = true;
    SDL_Event event;
    unsigned int generation = 0;
    
    while (running) {
        /* Process events */
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                    
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        handle_mouse_click(ctx, event.button.x, event.button.y);
                    }
                    break;
                    
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_SPACE) {
                        /* Pause/unpause with spacebar */
                        SDL_Delay(500); /* Simple pause mechanism */
                    } else if (event.key.keysym.sym == SDLK_r) {
                        /* Reset grid */
                        if (strcmp(ctx->config.config_type, CONFIG_RANDOM) == 0) {
                            initialize_grid_random(ctx);
                        }
                        generation = 0;
                    }
                    break;
            }
        }
        
        /* Render current state */
        render_grid(ctx);
        
        /* Advance simulation */
        simulate_step(ctx);
        generation++;
        
        /* Check if we should stop */
        if (ctx->config.steps > 0 && generation >= ctx->config.steps) {
            running = false;
        }
        
        /* Control frame rate */
        SDL_Delay(FRAME_DELAY_MS);
    }
    
    return GOL_SUCCESS;
}

/**
 * @brief Print usage information
 * @param program_name Name of the program executable
 */
static void print_usage(const char *program_name) {
    printf("Usage: %s <config_file>\n", program_name);
    printf("\nConfiguration file format:\n");
    printf("  @nrows <number>     - Number of grid rows\n");
    printf("  @ncols <number>     - Number of grid columns\n");
    printf("  @config <type>      - Configuration type (random|manual)\n");
    printf("  @steps <number>     - Number of steps (optional, 0 = infinite)\n");
    printf("  @seed <number>      - Random seed (optional, 0 = time-based)\n");
    printf("\nFor manual configuration, add:\n");
    printf("  @grid\n");
    printf("  <grid_rows>         - Grid pattern using 1/#/* for alive, 0/./<space> for dead\n");
    printf("\nExample manual config file:\n");
    printf("  @nrows 5\n");
    printf("  @ncols 5\n");
    printf("  @config manual\n");
    printf("  @grid\n");
    printf("  00100\n");
    printf("  00100\n");
    printf("  00100\n");
    printf("  00000\n");
    printf("  00000\n");
    printf("\nControls:\n");
    printf("  Left click          - Toggle cell state\n");
    printf("  Space               - Pause/unpause\n");
    printf("  R                   - Reset grid (random configs only)\n");
    printf("  Close window        - Exit\n");
}

/**
 * @brief Main function
 */
int main(int argc, char *argv[]) {
    if (argc != 2) {
        print_usage(argv[0]);
        return GOL_ERROR_ARGS;
    }
    
    gol_context_t ctx = {0};
    gol_result_t result;
    
    /* Parse configuration */
    result = parse_config_file(argv[1], &ctx.config);
    if (result != GOL_SUCCESS) {
        return result;
    }
    
    /* Set up game context */
    ctx.rows = ctx.config.rows;
    ctx.cols = ctx.config.cols;
    
    /* Allocate grid */
    result = allocate_grid(&ctx);
    if (result != GOL_SUCCESS) {
        fprintf(stderr, "Error: Failed to allocate grid memory\n");
        return result;
    }
    
    /* Initialize grid based on configuration */
    if (strcmp(ctx.config.config_type, CONFIG_RANDOM) == 0) {
        initialize_grid_random(&ctx);
    } else if (strcmp(ctx.config.config_type, CONFIG_MANUAL) == 0) {
        initialize_grid_manual(&ctx, argv[1]);
    } else {
        fprintf(stderr, "Error: Unknown configuration type '%s'\n", 
                ctx.config.config_type);
        deallocate_grid(&ctx);
        return GOL_ERROR_CONFIG;
    }
    
    /* Initialize SDL */
    result = initialize_sdl(&ctx);
    if (result != GOL_SUCCESS) {
        deallocate_grid(&ctx);
        return result;
    }
    
    /* Run the simulation */
    result = run_simulation(&ctx);
    
    /* Cleanup */
    cleanup_sdl(&ctx);
    deallocate_grid(&ctx);
    
    return result;
}

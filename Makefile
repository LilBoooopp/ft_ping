#───────────────────────────  PROJECT BASICS  ────────────────────────────────#
NAME = template
CC   = cc
INC = include
RM   = rm -rf
FLAGS = -Werror -Wextra -Wall -g -I$(INC) #-fsanitize=address
MAKE := make --no-print-directory

#────────────────────────────  LIBFT SECTION  ────────────────────────────────#
LIBFT_A   = libft.a
LIBFT_DIR = libft/
LIBFT     = $(addprefix $(LIBFT_DIR), $(LIBFT_A))
PRINTF    = $(addprefix $(PRINTF_DIR), $(PRINTF_A))

#--------------------------------------SOURCES---------------------------------#
SRC_DIR = src/
SRC_SRC =  \
	main.c
SRC = $(addprefix $(SRC_DIR), $(SRC_SRC))

ALL_SRC = $(SRC) 
vpath %.c src 
#--------------------------------------OBJECTS----------------------------------#
OBJ_DIR  = Objects/
OBJECTS  = $(patsubst %.c,$(OBJ_DIR)%.o,$(notdir $(ALL_SRC)))
VPATH    = $(SRC_DIR)

#────────────────────────────  ANIMATION CONFIG  ─────────────────────────────#
ANIMATION_FRAMES = ⠋ ⠙ ⠹ ⠸ ⠼ ⠴ ⠦ ⠧ ⠇ ⠏
ANIMATION_COLOR  = '\033[1;36m'
TOTAL_FILES := $(words $(OBJECTS))

#───────────────────────────────  COLOR CODES  ───────────────────────────────#
NONE='\033[0m'
GREEN='\033[32m'
YELLOW='\033[33m'
GRAY='\033[2;37m'
CURSIVE='\033[3m'
BLUE='\033[34m'
MAGENTA='\033[35m'
CYAN='\033[36m'
WHITE='\033[37m'
BOLD='\033[1m'

#────────────────────────────────  RULES  ─────────────────────────────────────#
all: reset_counter $(OBJ_DIR) $(LIBFT) $(NAME)
	@git submodule update --init --recursive

reset_counter:
	@rm -f .counter
	@printf "0" > .counter

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)
	@printf $(BOLD)$(MAGENTA)"Objects directory created\n"$(NONE)

$(LIBFT):
	@printf $(CURSIVE)$(GRAY)"🔧 Making libft...\n"$(NONE)
	@$(MAKE) -C $(LIBFT_DIR)

$(NAME): $(OBJECTS)
	@$(CC) $(FLAGS) $(OBJECTS) $(LIBFT) $(PRINTF) -o $(NAME)
	@printf "\033[1;32m\n✅ $(NAME) successfully compiled!\n\033[0m"
	@rm .counter

#────────────────────────────  COMPILATION RULE  ─────────────────────────────#
$(OBJ_DIR)%.o: %.c
	@mkdir -p $(OBJ_DIR)
	@if [ ! -f .counter ]; then printf "0" > .counter; fi
	@file_count=$$(cat .counter); \
	file_count=$$((file_count + 1)); \
	printf $$file_count > .counter; \
	frames="⠋ ⠙ ⠹ ⠸ ⠼ ⠴ ⠦ ⠧ ⠇ ⠏"; \
	frame_index=$$((file_count % 10)); \
	frame=$$(printf $$frames | cut -d ' ' -f $$((frame_index + 1))); \
	percent=$$((100 * file_count / $(TOTAL_FILES))); \
	barlen=30; \
	done=$$((barlen * percent / 100)); \
	todo=$$((barlen - done)); \
	bar=$$(printf "█%.0s" $$(seq 1 $$done)); \
	space=$$(printf "░%.0s" $$(seq 1 $$todo)); \
	printf "\r\033[1;36m%s \033[1mCompiling\033[0m [%-*s] %3d%% \033[36m%-40.40s\033[0m" "$$frame" "$$barlen" "$$bar$$space" "$$percent" "$<"; \
	$(CC) $(FLAGS) -c $< -o $@

clean:
	@printf $(CURSIVE)$(GRAY)" -> Cleaning object files..\n"$(NONE)
	@$(RM) $(OBJ_DIR)
	@$(RM) .counter
	@$(MAKE) -C $(LIBFT_DIR) clean

fclean: clean
	@printf $(CURSIVE)$(GRAY)" -> Removing $(NAME)\n"$(NONE)
	@$(RM) $(NAME)
	@$(MAKE) -C $(LIBFT_DIR) fclean

re: fclean all

bonus:
	@printf $(CURSIVE)$(GRAY)" - Compiling bonus $(NAME)...\n"$(NONE)
	@printf $(GREEN)"- Compiled -"$(NONE)

.PHONY: all clean fclean re bonus reset_counter

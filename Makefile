CC = gcc
CFLAGS = -Wall -Wextra -pthread -Iinclude -Itests/vendor
LDFLAGS = -pthread

# Diretórios
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
TEST_DIR = tests
BIN_DIR = bin

# Arquivos
SOURCES = $(shell find $(SRC_DIR) -name '*.c')
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES))
# Objetos para os testes (exclui o main.o do simulador)
TEST_OBJECTS = $(filter-out $(OBJ_DIR)/main.o, $(OBJECTS))

TEST_SOURCES = $(wildcard $(TEST_DIR)/*.test.c)
TEST_BINS = $(patsubst $(TEST_DIR)/%.test.c, $(BIN_DIR)/%, $(TEST_SOURCES))

# Bibliotecas de terceiros (Unity)
UNITY_SRC = $(TEST_DIR)/vendor/unity.c

# Alvo principal (Simulador)
TARGET = $(BIN_DIR)/simulador

# Alvos de execução (Padrão: make run <alvo>)
# Este bloco permite usar 'make run test', 'make run lint', etc.
ifeq (run,$(firstword $(MAKECMDGOALS)))
  RUN_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  $(foreach arg,$(RUN_ARGS),$(if $(filter $(arg),$(shell grep -oE '^[a-zA-Z0-9_-]+:' Makefile | sed 's/://')),,$(eval $(arg):;@:)))
endif

run: $(TARGET)
	@if [ "$(RUN_ARGS)" = "test" ]; then \
		$(MAKE) --no-print-directory test; \
	elif [ "$(RUN_ARGS)" = "lint" ]; then \
		$(MAKE) --no-print-directory lint; \
	elif [ "$(RUN_ARGS)" = "format" ]; then \
		$(MAKE) --no-print-directory format; \
	elif [ -z "$(RUN_ARGS)" ]; then \
		./$(TARGET); \
	else \
		echo "Uso: make run [test|lint|format] ou apenas make run para o simulador"; \
	fi

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Regra para compilar e rodar todos os testes automaticamente
test: $(TEST_BINS)
	@for test in $(TEST_BINS); do \
		echo "Executando $$test..."; \
		./$$test; \
	done

# Compilação de cada arquivo de teste
$(BIN_DIR)/%: $(TEST_DIR)/%.test.c $(TEST_OBJECTS) $(UNITY_SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

lint:
	cppcheck --enable=all --suppress=missingIncludeSystem -Iinclude src/ include/

format:
	clang-format -i $(SOURCES) include/*.h $(TEST_SOURCES)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all test lint format clean run
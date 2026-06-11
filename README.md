# PthreadTraffic

Simulador concorrente em C utilizando Pthreads para representar o fluxo de veículos em uma malha viária ASCII.

## 🏗️ Estrutura do Projeto

O projeto segue uma estrutura de **Monolito Modular**, onde cada subsistema é isolado em sua própria pasta.

```text
.
├── Makefile                # Automação de compilação
├── README.md               # Documentação e regras
├── TASKS.md                # Cronograma e tarefas
├── .gitignore              # Arquivos ignorados pelo Git
├── map.txt                 # Configuração da malha viária (entrada)
├── docs/                   # Especificações e design
│   └── initial_design.md   # Design inicial do projeto
├── src/
│   ├── main.c              # Inicialização e orquestração
│   └── modules/            # Núcleo Modular
│       ├── map/            # Módulo de Mapa
│       │   ├── map.c
│       │   └── map.h
│       ├── clock/          # Módulo de Relógio
│       │   ├── clock.c
│       │   └── clock.h
│       ├── vehicle/        # Módulo de Veículos
│       │   ├── vehicle.c
│       │   └── vehicle.h
│       ├── traffic/        # Módulo de Semáforos
│       │   ├── traffic.c
│       │   └── traffic.h
│       └── display/        # Módulo de Interface
│           ├── display.c
│           └── display.h
└── tests/                  # Testes Unitários
    ├── example.test.c
    └── vendor/             # Framework Unity
```

## 📜 Regras de Desenvolvimento

### Idioma e Documentação

- **Código:** Todo o código (nomes de arquivos, variáveis, funções) deve ser em **Inglês**.
- **Docstrings:** Todo arquivo deve ter um cabeçalho em **Português**:
  ```c
  /**
   * Descrição: O que este arquivo faz.
   * Autor: Nome do Autor
   */
  ```

### Git e Fluxo de Trabalho

- **Branches:** As funcionalidades devem ser desenvolvidas em branches `feature/` e enviadas para a `develop`. A branch `main` contém apenas código estável.
- **Commits:** Seguimos o padrão de **Conventional Commits** (Prefixo em Inglês, Mensagem em Português).
  - **Padrão:** `<prefixo>: <mensagem>`
  - **Exemplo Real:** `feat: adiciona lógica de movimento do veículo`
  - **Tipos comuns:**
    - `feat: ...` (Funcionalidades)
    - `fix: ...` (Correções)
    - `docs: ...` (Documentação)
    - `refactor: ...` (Refatoração)

## 🛠️ Comandos (Makefile)

| Comando       | Descrição                       |
| :------------ | :------------------------------ |
| `make`        | Compila o projeto               |
| `make run`    | Executa o simulador             |
| `make test`   | Executa os testes unitários     |
| `make lint`   | Executa o linter (cppcheck)     |
| `make format` | Formata o código (clang-format) |
| `make clean`  | Remove arquivos de compilação   |

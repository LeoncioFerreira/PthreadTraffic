# Especificação de Design: Simulador de Tráfego Urbano

## 1. Objetivo

Desenvolver um simulador de tráfego urbano concorrente em C utilizando a biblioteca Pthreads. O foco é a aplicação de conceitos de sincronização, exclusão mútua e prevenção de deadlocks através de uma malha viária representada em ASCII.

## 2. Arquitetura: Monolito Modular

O projeto seguirá uma estrutura de monolito modular, onde cada subsistema é isolado em seu próprio par de arquivos `.c` e `.h`, comunicando-se através de APIs claras.

### 2.1 Módulos Identificados

1.  **Módulo de Mapa (`map.h/c`):** Responsável por ler o `.txt`, inicializar a matriz de structs e os mutexes das células.
2.  **Módulo de Relógio (`clock.h/c`):** Gerencia a thread do tempo global e as variáveis de condição de sincronização de ticks.
3.  **Módulo de Veículos (`vehicle.h/c`):** Define o comportamento das threads dos carros, lógica de movimento e prioridade da ambulância.
4.  **Módulo de Sinalização (`traffic.h/c`):** Gerencia semáforos e lógica de cruzamentos.
5.  **Módulo de Interface (`display.h/c`):** Funções para limpar o terminal e desenhar a matriz ASCII.

### 2.2 Estrutura de Pastas Sugerida

```text
/
├── src/            # Implementação (.c)
├── include/        # Cabeçalhos (.h)
├── tests/          # Testes Unitários (Unity)
├── scripts/        # Scripts de automação e lint
├── Makefile        # Automação de compilação
└── mapa.txt        # Configuração da malha viária
```

## 3. Pipeline de Qualidade e Ferramentas

Para garantir que a equipe (Leoncio, Paulo, Salomão e André) mantenha a integridade do código:

- **Linter:** `cppcheck` - Verificação estática para evitar memory leaks e bugs de concorrência.
- **Formatação:** `clang-format` - Estilo de código unificado.
- **Testes Unitários:** `Unity Test` - Para validar a lógica de cada módulo isoladamente.
- **CI (GitHub Actions):** Automação que executa o linter e os testes a cada `push`.

## 3. Mecanismos de Sincronização (Requisitos da Tabela)

| Mecanismo         | Uso no Projeto                                                                  |
| :---------------- | :------------------------------------------------------------------------------ |
| **Mutex**         | Um mutex por célula da matriz para garantir a **Impenetrabilidade**.            |
| **Var. Condição** | 1. Sincronização com o Relógio Global. 2. Bloqueio em Semáforos Vermelhos.      |
| **Semáforo**      | Controle de fluxo em cruzamentos críticos (limite de veículos na área central). |
| **Threads**       | Representação individual de cada veículo e do relógio.                          |

## 4. Regras de Negócio e Movimentação

### 4.1 Velocidades e Ticks

- **Carro Rápido:** Move a cada 1 tick.
- **Carro Médio:** Move a cada 2 ticks.
- **Carro Lento:** Move a cada 4 ticks.
- A movimentação só ocorre se `tick_atual % velocidade == 0`.

### 4.2 Semáforos e Ambulância

- **Semáforos:** Alternam entre Verde/Vermelho via thread controladora ou temporização. Carros bloqueiam em `pthread_cond_wait` se o sinal for vermelho.
- **Ambulância:** Possui prioridade. Ao se aproximar de um cruzamento, ela altera o estado do semáforo para verde (se seguro) e registra a solicitação em log.

### 4.3 Prevenção de Deadlocks

Estratégia: **Ordenação de Recursos**.
Para atravessar um cruzamento, o veículo deve tentar adquirir os mutexes de todas as células necessárias para a travessia de uma vez (All-or-Nothing). Caso não consiga todas, ele libera as que já adquiriu e espera.

## 5. Cronograma de Implementação (3 Semanas)

### Semana 1: Motor Principal

- Implementação do parser de `mapa.txt`.
- Criação da thread Clock e lógica de ticks com Variáveis de Condição.
- Movimentação básica de 1 carro com Mutex de célula.

### Semana 2: Controle de Tráfego

- Implementação de múltiplos carros (10-20).
- Lógica de Semáforos e Cruzamentos.
- Implementação da Ambulância e sistema de prioridade.

### Semana 3: Estabilidade e Finalização

- Refinamento da estratégia anti-deadlock.
- Polimento da interface ASCII.
- Escrita do Relatório Final e documentação.

## 6. Critérios de Sucesso

- Nenhum "teletransporte" ou colisão (dois carros na mesma célula).
- Respeito total aos semáforos sem consumo excessivo de CPU (Busy Waiting).
- Ambulância atravessando com prioridade comprovada nos logs.
- Simulação rodando por tempo indefinido sem travamentos (Deadlocks).

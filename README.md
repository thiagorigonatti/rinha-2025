[![](https://img.shields.io/discord/677642178083946580?color=%23768ACF&label=Discord)](https://discord.gg/U8NcPcHxW3)
[![](https://img.shields.io/badge/Linked-In-blue)](https://www.linkedin.com/in/thiagorigonatti/)

# 🐔 Rinha de Backend 2025 - Implementação em C
##### Por: Thiago Rigonatti - Engenheiro de software especialista.
Este projeto é uma implementação **full low-level** para a [Rinha de Backend](https://github.com/zanfranceschi/rinha-de-backend), focada em **alta performance**, **baixa latência** e **controle total sobre recursos**.

Aqui não tem framework mágico: é C puro, controle manual de memória, IPC com memória compartilhada e filas lock-free, servidor de sockets minimalista e um cliente HTTP otimizado - tudo orquestrado com **Docker**, **HAProxy** e **docker-compose**.

---

## 🔍 Arquitetura e Abordagens

### 1. **C puro + controle total**
O código foi escrito inteiramente em C para:
- Evitar overhead de linguagens interpretadas ou VMs.
- Ter acesso direto à memória e ao sistema operacional.
- Garantir previsibilidade no consumo de CPU e RAM.
- Manipular o agendamento de threads e afinidade das cpus.
- Destaque para o baixíssimo uso de CPU e RAM.
---

### 2. **Fila Lock-Free (`lock_free_queue.c`)**
Implementação de fila sem bloqueio para evitar **mutexes** e **spinlocks**, usando **operações atômicas**.

Vantagens:
- Menos context switches.
- Procura outra posição na fila, invés de ficar esperando locks.
- Melhor performance sob alta concorrência.
- Evita *priority inversion*.

---

### 3. **Memória Compartilhada (`shm_array.c`)**
Uso de **shm_open** e **mmap** para criar e acessar dados entre processos sem cópia redundante.

Vantagens:
- Comunicação mais rápida do que via TCP para processos locais (entre contêiners).
- Evita serialização/deserialização desnecessária.

---

### 4. **Servidor HTTP (`http_server.c`)**
Implementa um servidor HTTP otimizado que usa unix domain sockets:
- Uso de `epoll` para I/O não-bloqueante.
- Buffer fixo para evitar alocações dinâmicas frequentes.
- Manipulação direta do protocolo HTTP.

---

### 5. **Cliente HTTP (`http_client.c`)**
Cliente HTTP interno para comunicação com outros serviços:
- Conexões persistentes (keep-alive).
- Parsing mínimo para velocidade máxima.

---

### 6. **Load Balancer HAProxy**
- haproxy.cfg: configura round-robin e health checks (`check`).

---

### 7. **Orquestração com Docker**
- Dockerfile: build otimizado, usando estágio multi-stage from scratch.
- docker-compose.yml: define múltiplas instâncias, memória compartilhada e o balanceador de carga.

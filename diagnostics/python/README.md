# Nodo Python Diagnostics

Esta pasta não substitui os testes C++ do projeto.

Ela existe para investigar melhor por que estes testes estão falhando:

```text
app_CommandLineLocalFlowTests
app_CommandLinePersistentMempoolTests
app_CommandLineRuntimeBlockTests
node_FinalizedBlockStoreTests
node_RuntimeStateLoaderTests
```

## Como usar

Copie a pasta `diagnostics/python` para a raiz do projeto `nodo`.

Depois, na raiz do projeto, rode:

```bash
python3 diagnostics/python/run_failed_tests_diagnostics.py
```

Ou, se quiser rodar via `unittest`:

```bash
python3 -m unittest discover -s diagnostics/python/tests -v
```

## O que o diagnóstico faz

Ele gera relatórios em:

```text
diagnostics/reports/
```

Com dois arquivos:

```text
nodo_failure_diagnostics_<timestamp>.md
nodo_failure_diagnostics_<timestamp>.json
```

O diagnóstico tenta descobrir:

- quais dos 5 testes ainda falham;
- qual comando `ctest` foi executado;
- saída completa do teste com `--output-on-failure -VV`;
- se `FinalizedBlockStore.cpp` e `RuntimeStateLoader.cpp` estão usando a mesma versão de bloco;
- quais campos são persistidos pelo `FinalizedBlockStore`;
- quais campos o `RuntimeStateLoader` permite/parseia;
- campos persistidos mas não aceitos no reload;
- erros conhecidos como:
  - campo vazio;
  - campo desconhecido;
  - versão divergente;
  - recompensa real de proteção não persistida;
  - governance não persistida;
  - fee split divergente;
  - monetary firewall divergente;
  - slashing summary divergente.

## Importante

Esta pasta é somente para diagnóstico.

Ela não altera arquivos do protocolo, não mexe no banco, não aplica migrations e não corrige código automaticamente.

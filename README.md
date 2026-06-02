# Nodo

Nodo is an experimental C++ foundation for the Nodo Protocol, a
Proof-of-Protection blockchain protocol focused on network protection,
auditable state transitions, safe finality and damage reduction when nodes or
validators misbehave.

Nodo is not a production mainnet. The current `localnet` path is intentionally
small, but it uses the same protocol pipeline that future `testnet` and
`mainnet` configurations should use: transaction admission, candidate block
production, state-transition validation, validator votes, quorum certificate,
finalization, persistence, reload and chain audit.

## Current Shape

- `apps/cli/`: CLI entrypoint.
- `include/` and `src/`: modules for app, core, consensus, crypto, economics,
  mempool, node runtime, privacy, serialization, storage and P2P foundations.
- `tests/<module>/`: CTest-discovered module tests.
- `docs/`: stable project docs plus detailed module guides.

## Quickstart

```bash
./scripts/cmake_build.sh
./scripts/cmake_test_all.sh
```

The CLI binary is written to:

```text
build/nodo
```

## Localnet Flow

```bash
build/nodo init --data-dir .nodo
build/nodo keys create --data-dir .nodo
build/nodo tx submit --data-dir .nodo
build/nodo block produce --data-dir .nodo
build/nodo node reload --data-dir .nodo
build/nodo chain audit --data-dir .nodo
build/nodo status --data-dir .nodo
build/nodo inspect --data-dir .nodo
```

Compatibility commands such as `submit-demo-transaction`, `produce-demo-block`
and `reload` still exist while operator muscle memory migrates. They print
deprecation warnings and run the same localnet protocol path.

Current limitations are explicit:

- `localnet` uses OpenSSL Ed25519 for user transactions and blst BLS12-381 for
  validator operations;
- `localnet` keys are stored by `KeyStore` in an unencrypted deterministic
  local format that is not production-safe custody yet;
- `localnet` uses explicit development account allocations in `GenesisConfig`
  for the default user key so balance and nonce checks can run locally;
- P2P, TCP transport, gossip and encrypted peer-channel foundations exist for
  testnet development, but they are not a production networking stack yet;
- slashing evidence and validator penalty decisions are auditable and
  idempotent, but production stake-slashing and mainnet activation are still
  intentionally out of scope;
- mempool future nonces are rejected until a full per-account queue exists;
- coin-lot ownership, double-spend and complete supply audit must continue
  moving behind the state-transition validation gate.

The runtime manifest stores `latestStateRoot`, and reload rebuilds state from
genesis through finalized blocks before accepting the manifest tip. Chain audit
reports the same root together with finalized height and hash.

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Protocol](docs/PROTOCOL.md)
- [State transition](docs/STATE_TRANSITION.md)
- [Consensus rules](docs/CONSENSUS_RULES.md)
- [Security model](docs/SECURITY_MODEL.md)
- [Networks](docs/NETWORKS.md)
- [CLI](docs/CLI.md)
- [Node data directory](docs/NODE_DATA_DIRECTORY.md)
- [Development mode](docs/DEVELOPMENT_MODE.md)
- [Roadmap](docs/ROADMAP.md)

O Nodo deve ser simples, modular e verificável. Cada parte do protocolo precisa existir por uma razão clara: proteger a rede, preservar a integridade do estado ou tornar a auditoria mais confiável. Nenhum recurso deve ser adicionado apenas por parecer moderno ou sofisticado; ele só deve entrar se contribuir diretamente para o Proof-of-Protection.
O Nodo deve priorizar a simplicidade nas regras fundamentais. Um node honesto precisa conseguir verificar blocos, transações, saldos, penalidades, recompensas e evidências sem depender de confiança externa. Tudo que altera o estado da rede deve ser reproduzível, determinístico e auditável. A segurança monetária deve vir antes de qualquer expansão: não pode haver inflação indevida, gasto duplo, saldo sem origem ou divergência entre o estado final e o histórico que o produziu.
O Nodo deve ser modular para que a proteção não fique espalhada de forma confusa pelo código. A rede deve ter módulos claros para evidências, auditoria, recompensas, penalidades, disponibilidade de dados, reputação de peers, validação de blocos e segurança de validadores. Cada módulo deve ter responsabilidade própria, entrada bem definida, saída verificável e regras fáceis de testar.
O Nodo deve transformar proteção em trabalho comprovável. Validadores não devem ser recompensados apenas por participar da rede, mas por demonstrar que ajudaram a protegê-la. Essa proteção pode envolver auditoria de estado, propagação correta de dados, detecção de mensagens inválidas, isolamento de peers maliciosos, manutenção de disponibilidade, verificação de evidências e recuperação de falhas. Toda ação de proteção relevante deve gerar uma prova, um registro ou uma evidência que possa ser verificada depois.
O Nodo deve tratar segurança como um processo contínuo, não como uma promessa. A rede precisa resistir a spam, sobrecarga, peers maliciosos, mensagens inválidas, falhas de storage, votos contraditórios, blocos inválidos e tentativas de corromper o estado. Quando um comportamento perigoso for detectado, ele deve ser convertido em evidência canônica. Essa evidência deve poder gerar penalidade, perda de reputação ou redução de influência, sempre de forma verificável e sem punição duplicada pelo mesmo fato.
O Nodo deve usar imprevisibilidade como defesa. Auditorias, desafios e verificações não devem ser totalmente previsíveis, porque previsibilidade facilita ataques direcionados. A rede deve poder sortear validadores ou comitês para verificar blocos, peers, dados, evidências e disponibilidade. Quanto mais difícil for prever quem vai auditar o quê, mais difícil será manipular a proteção.
O Nodo deve proteger também a disponibilidade dos dados. Um bloco não é realmente seguro se seus dados não podem ser recuperados, verificados e auditados. A rede deve incentivar nodes a manter, servir e provar disponibilidade de informações críticas, especialmente blocos finalizados, evidências de ataque, roots de estado, registros de penalidade e histórico necessário para reconstrução.
O Nodo deve separar identidade, consenso, rede e proteção. Uma única chave não deve controlar todas as funções críticas. A identidade do validador, a assinatura de votos, a comunicação de rede, a custódia de fundos e as atestações de proteção devem ser separadas sempre que possível. Isso reduz o impacto de comprometimento e permite respostas mais seguras a falhas operacionais.
O Nodo deve ser auditável após falhas. Reiniciar um node, recarregar storage ou reconstruir estado nunca deve depender de suposições frágeis. A rede deve rejeitar dados ambíguos, arquivos corrompidos, versões desconhecidas e qualquer estado que não possa ser reconstruído de forma determinística. Se o estado final não bate com o histórico, o estado deve ser rejeitado.
O Nodo deve evoluir com cuidado. Novas regras de proteção, mudanças econômicas, formatos de storage e mecanismos de penalidade precisam passar por teste, simulação, revisão e ativação controlada. A pressa para adicionar recursos não pode ser maior que a responsabilidade de manter a rede segura.
O Nodo deve preservar privacidade quando isso fortalecer a segurança. Nem toda evidência precisa expor mais informação do que o necessário. Sempre que possível, a rede deve provar validade, auditoria ou comportamento correto com exposição mínima de dados sensíveis. O objetivo é punir ataques e recompensar proteção sem criar novos riscos desnecessários.
O Nodo deve seguir um princípio central: proteção precisa ser mensurável, verificável e útil. Se uma ação não pode ser medida, ela não deve gerar recompensa. Se uma denúncia não pode ser verificada, ela não deve gerar penalidade. Se um recurso não aumenta a segurança, a auditoria ou a resiliência da rede, ele deve esperar.
O Proof-of-Protection deve fazer do Nodo uma rede onde a segurança não é apenas ausência de falhas, mas uma atividade permanente. A rede deve recompensar quem protege, penalizar quem ameaça, registrar evidências, preservar dados críticos e permitir que qualquer node honesto audite o caminho completo do estado.

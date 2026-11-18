# Sobre o KMedia

**KMedia** é um aplicativo desktop multiplataforma de código aberto escrito em **C++20**, **Qt 6** e **libmpv** para baixar, organizar e apresentar áudio, vídeo, imagens e documentos PDF em reuniões congregacionais.

## Por Que Criei o KMedia

Apresentadores baseados em navegadores embarcados consomem centenas de megabytes de RAM, abrem múltiplos processos em segundo plano e provocam aquecimento e ruído de ventoinha em notebooks mais antigos do Salão do Reino. Construí o KMedia do zero para entregar gestão completa de mídia com consumo mínimo de recursos:

- **Baixo Uso de Memória:** Meta abaixo de 45 MB de RAM em repouso e abaixo de 75 MB durante reprodução de vídeo 1080p.
- **Hardware Frio e Silencioso:** Decodificação por hardware na GPU (`hwdec=auto-safe` via `libmpv`) e arquitetura orientada a eventos sem loops de polling.
- **Inicialização Instantânea:** Persistência SQLite3 em modo WAL e cache em disco de miniaturas para abertura e troca de perfis quase instantâneas.
- **Tema Studio Carbon & Broadcast Orange:** Projetado para baixo cansaço visual e zero reflexo em auditórios pouco iluminados.

## Recursos Principais

- **Mídias das Reuniões:** Organiza imagens, vídeos, cânticos e publicações semanais automaticamente por data e idioma.
- **Pacotes e Documentos:** Importa arquivos `.jwpub`, `.jwlplaylist`, pacotes de discurso público `S-34` e `S-34mp`, mídia local e PDFs multipágina.
- **Controles Precisos:** Marcadores não destrutivos de início e fim, zoom e deslocamento sincronizados em imagens/PDFs, ordenação por arrastar e soltar e Standby instantâneo.
- **Integrações Híbridas:** Controle nativo via OBS WebSocket v5, abertura de reuniões Zoom (`zoommtg://`), seleção isolada de dispositivo de áudio e detecção leve de processos.
- **Interface Multilíngue:** Traduções da interface via Qt Linguist independentes do idioma das mídias baixadas.

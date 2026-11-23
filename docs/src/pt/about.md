# Sobre o KMedia

**KMedia** é um aplicativo desktop multiplataforma de código aberto escrito em **C++20**, **Qt 6** e **libmpv** para baixar, organizar e apresentar áudio, vídeo, imagens e documentos PDF em reuniões congregacionais.

## Por Que Criei o KMedia

Apresentadores baseados em navegadores embarcados consomem centenas de megabytes de RAM, abrem múltiplos processos em segundo plano e provocam aquecimento e ruído de ventoinha em notebooks mais antigos do Salão do Reino. Construí o KMedia do zero para entregar gestão completa de mídia com consumo mínimo de recursos:

- **Baixo Uso de Memória:** Cerca de 135 MB de memória privada depois de aberto no Windows, num único processo e sem navegador embutido.
- **Hardware Frio e Silencioso:** Decodificação por hardware na GPU (`hwdec=auto-safe` via `libmpv`) e arquitetura orientada a eventos sem loops de polling.
- **Inicialização Rápida:** A janela fica pronta em cerca de 1,3 segundo numa abertura a quente, com SQLite3 em modo WAL e cache em disco de miniaturas.
- **Tema Studio Carbon & Broadcast Orange:** Projetado para baixo cansaço visual e zero reflexo em auditórios pouco iluminados.

## Recursos Principais

- **Mídias das Reuniões:** Organiza imagens, vídeos, cânticos e publicações semanais automaticamente por data e idioma.
- **Pacotes e Documentos:** Importa arquivos `.jwpub`, `.jwlplaylist`, pacotes de discurso público `S-34` e `S-34mp`, mídia local e PDFs multipágina.
- **Controles Precisos:** Marcadores não destrutivos de início e fim, zoom e deslocamento sincronizados em imagens/PDFs, ordenação por arrastar e soltar e Standby instantâneo.
- **Integrações Híbridas:** Controle nativo via OBS WebSocket v5, abertura de reuniões Zoom (`zoommtg://`) e detecção leve de processos.
- **Interface Multilíngue:** Traduções da interface via Qt Linguist independentes do idioma das mídias baixadas.

## Inspiração

O fluxo dia a dia das reuniões no KMedia é inspirado no [Meeting Media Manager](https://github.com/sircharlo/meeting-media-manager), o aplicativo de código aberto que muitas congregações já usam. O KMedia é um aplicativo nativo independente, escrito do zero, e não tem o objetivo de concorrer com ele.

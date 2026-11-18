---
layout: home

hero:
  name: KMedia
  text: Apresentador Nativo de Mídia para Reuniões
  tagline: Construído do zero em C++20, Qt 6 e libmpv para operação silenciosa, menos de 45 MB de RAM em repouso e reprodução acelerada por GPU em duas telas.
  actions:
    - theme: brand
      text: Baixar
      link: /pt/download
    - theme: alt
      text: Sobre
      link: /pt/about
    - theme: alt
      text: Manual do Usuário
      link: /pt/user-guide
    - theme: alt
      text: Configurações
      link: /pt/settings-guide
    - theme: alt
      text: Perguntas Frequentes
      link: /pt/faq

features:
  - title: Desempenho Nativo Leve
    details: Projetado para consumir menos de 45 MB de RAM em repouso, inicializar em menos de 400 ms e manter notebooks frios e silenciosos.
  - title: Motor libmpv com Aceleração por GPU
    details: Renderização OpenGL sem cópia com decodificação por hardware automática (hwdec=auto-safe), áudio contínuo e cortes sem recodificação.
  - title: Download Automático de Mídias
    details: Busca catálogos e mídias semanais em centenas de idiomas com cache por ETag e limite educado de downloads simultâneos.
  - title: Leitura Nativa de JWPUB, Playlists e PDF
    details: Extrai pacotes .jwpub e .jwlplaylist diretamente em C++ e renderiza documentos PDF multipágina via Qt6::Pdf.
  - title: Apresentação em Duas Telas
    details: Detecta monitores externos automaticamente, projeta sem bordas na segunda tela e oferece modo Standby ou Blackout instantâneo.
  - title: Música Ambiente Programada
    details: Reproduz música ambiente antes e depois das reuniões com redução gradual de volume em 5 segundos no horário agendado.
  - title: Perfis para Múltiplas Congregações
    details: Mantém bancos SQLite WAL, pastas de mídia, horários e ajustes de OBS/Zoom isolados por congregação com troca em menos de 100 ms.
  - title: Integração com OBS Studio e Zoom
    details: Cliente nativo OBS WebSocket v5 para troca de cenas, detecção leve de processos, áudio dedicado e abertura direta de reuniões Zoom.
  - title: Sincronização com Pastas na Nuvem
    details: Monitora pastas locais do Dropbox, OneDrive, Google Drive e Nextcloud via notificações do sistema operacional sem varredura contínua.
---

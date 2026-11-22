---
layout: home

hero:
  name: KMedia
  text: Apresentador Nativo de Mídia para Reuniões
  tagline: Construído do zero em C++20, Qt 6 e libmpv para operação silenciosa, menos de 45 MB de RAM em repouso e reprodução acelerada por GPU em duas telas.
  image:
    src: /logo.svg
    alt: Logo do KMedia
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
  - title: Página da Reunião Dia a Dia
    details: Abre cada dia de reunião seção por seção, com miniaturas reais e um calendário cujos pontos coloridos mostram quais dias estão prontos.
  - title: Música Ambiente com o Cancioneiro
    details: Toca o cancioneiro no idioma das mídias antes das reuniões, com o fim marcado para o começo da reunião e os cânticos do dia por último.
  - title: Perfis para Múltiplas Congregações
    details: Mantém bancos SQLite WAL, pastas de mídia, horários e ajustes de OBS/Zoom isolados por congregação com troca em menos de 100 ms.
  - title: Integração com OBS Studio e Zoom
    details: Cliente nativo OBS WebSocket v5 para troca de cenas, detecção leve de processos e abertura direta de reuniões Zoom.
  - title: Pasta Compartilhada e Exportação de Mídias
    details: Traz para cada dia as mídias das pastas com data do Dropbox, OneDrive, Google Drive ou Nextcloud e exporta cada dia na ordem da reunião.
---

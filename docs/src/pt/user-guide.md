# Manual do Usuário

Este manual apresenta o fluxo principal do KMedia para preparar e exibir mídias das reuniões.

## Console de Três Colunas

1. **Barra Lateral Esquerda:** Alterne entre perfis de congregações, navegue entre *Reunião*, *Mídia*, *Playlists*, *Downloads* e *Configurações*, e acompanhe em tempo real o status do OBS Studio, Zoom, segundo monitor e espaço em disco.
2. **Área Central da Reunião:** Gerencie a lista de reprodução ativa, reorganize itens arrastando e soltando, adicione arquivos locais ou pacotes `.jwpub`/`.jwlplaylist` e controle a reprodução pela barra inferior.
3. **Painel Direito de Prévia e Inspetor:** Acompanhe a saída enviada ao projetor, ative *Standby / Blackout* com um clique, confira metadados técnicos, defina pontos de corte de início/fim ou controle zoom, deslocamento e páginas de imagens e PDFs.

## Reprodução, Cortes, Zoom e Apresentação de PDFs

- **Cortes Não Destrutivos:** Os marcadores de início (`trimStartMs`) e fim (`trimEndMs`) são aplicados diretamente no `libmpv` sem alterar nem recodificar o arquivo original. Se o tempo final for menor ou igual ao inicial, o limite final é desconsiderado.
- **Controle de Zoom e Pan:** Ajuste o zoom (`1.0x` até `8.0x`) e o deslocamento horizontal/vertical (`-0.5` a `+0.5`) em vídeos, imagens e documentos PDF sincronizados com a saída externa, com opção de reset instantâneo.
- **Documentos PDF Multipágina:** Avance, retorne ou salte diretamente para qualquer página renderizada nativamente via `Qt6::Pdf` na resolução do monitor de exibição.

## Música Ambiente e Horários das Reuniões

- A música ambiente opera em um pipeline exclusivo de áudio do `libmpv`, totalmente isolado do player principal de apresentações.
- Configure os dias e horários das reuniões de meio e fim de semana para que a música ambiente realize automaticamente um fade-out suave (padrão `5000 ms`) encerrando exatamente no horário de início da reunião. Pausar ou retomar manualmente assume prioridade na sessão atual.

## Importação de Mídias

- **Arrastar e Soltar:** Arraste vídeos (`MP4`, `MKV`, `WebM`), áudios (`MP3`, `AAC`, `OGG`, `FLAC`), imagens (`JPEG`, `PNG`, `WebP`) ou documentos `PDF` diretamente para a playlist.
- **Pacotes de Publicações:** Importe arquivos `.jwpub` e `.jwlplaylist` nativamente sem precisar descompactar manualmente.
- **Pastas Sincronizadas na Nuvem:** Configure uma pasta local do Dropbox, OneDrive, Google Drive ou Nextcloud para importar novas mídias automaticamente.

## Apresentação em Segunda Tela

Quando um segundo monitor ou projetor está conectado, o KMedia o atribui automaticamente para exibição em tela cheia sem bordas (janela `"KMedia - Presentation"`), mantendo o console do operador na tela principal. O botão *Standby / Blackout* alterna instantaneamente entre a mídia ativa e uma tela preta ou imagem de espera da congregação sem perder o estado da fila.

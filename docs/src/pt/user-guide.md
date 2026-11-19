# Manual do Usuário

Este manual apresenta o fluxo principal do KMedia para preparar e exibir mídias das reuniões.

## Console Responsivo de Três Colunas

O KMedia se adapta automaticamente a telas largas de cabine, divisão 50/50 ou janela compacta lateral (`360x520` mínimo para trabalhar lado a lado com o Zoom) e permite alternar entre os temas **Studio Carbon (Escuro)** e **Studio Daylight (Claro)** via `Ctrl+D` ou pelo botão no topo da barra lateral:

1. **Modo Largo (`>= 1080 px`):** Exibe a barra lateral completa (`234 px`) com logotipo e seletor de congregação, a área central da reunião e a pilha de cartões do inspetor direito (`344 px`).
2. **Modo Médio (`680 px` a `1079 px`):** Recolhe a barra lateral para o modo compacto (`64 px`) e transforma a pré-visualização ao vivo em gaveta retrátil sob demanda.
3. **Modo Compacto (`< 680 px`, até `360x520`):** Move a navegação para abas inferiores compactas (`54 px`), resume a **Barra em Pílula (`ActionPillBar`)** a ícones com indicadores de status e mantém a barra de reprodução e acesso ao *Standby*.

![Console Desktop do KMedia (Tema Escuro)](../assets/kmedia-preview.png)

![Console Desktop do KMedia (Tema Claro)](../assets/kmedia-light-preview.png)

![Layout Compacto Lado a Lado do KMedia](../assets/kmedia-compact-preview.png)

1. **Barra Lateral / Abas Compactas:** Alterne perfis de congregação e o tema Claro/Escuro, navegue entre *Reunião*, *Calendário*, *Mídias*, *Playlists*, *Downloads* e *Configurações*, e acompanhe em tempo real o status de OBS, 2ª Tela, Zoom e armazenamento livre em disco.
2. **Divisão por Seções e Biblioteca Visual de Mídias:** Os itens da reunião ativa são agrupados sob cabeçalhos coloridos por seção (*Tesouros da Palavra de Deus*, *Faça Seu Melhor no Ministério*, *Nossa Vida Cristã*, *Discurso Público*, *A Sentinela* e *Visita do Superintendente*), com alça de arrastar-e-soltar, seletor rápido de reuniões/semanas (`< [ Midweek Meeting v ] >`) e controles completos de reprodução. Na aba *Mídias*, alterne entre grade visual de miniaturas 16:9 e lista compacta, filtre por tipo (*Tudo*, *Vídeo*, *Áudio*, *Imagem*, *PDF*) ou pesquise por nome com indicador compacto de pastas na nuvem.
3. **Calendário de Reuniões (`CalendarView`):** Visualize a grade mensal iniciando na segunda-feira com os dias configurados para reuniões de meio e fim de semana (`MWB` e *A Sentinela* com indicadores verdes de mídia em cache local), personalize os dias e horários da congregação (`Schedule`, persistidos por perfil e sincronizados com o fade-out da música ambiente), acompanhe as próximas quatro semanas e use **Open Week** ou **Sync Week** para abrir ou sincronizar a semana escolhida diretamente na reunião.
4. **Barra Flutuante em Pílula (`ActionPillBar`):** Conta com botões ampliados guiados por ícones (incluindo os emblemas de OBS Studio e Zoom nas cores do sistema), indicadores de status em LED, alternância imediata de *Espera / Blackout* e *Legendas* (`libmpv`), além de popovers ancorados para escolher o monitor de exibição, controlar música ambiente, acompanhar o cronômetro da reunião (`Decorrido` / `Restante` / `Excedido`), trocar cenas do OBS ao vivo, abrir o Zoom e gerenciar downloads.
5. **Paleta de Comandos (`Ctrl+K`) e Notificações:** Pressione `Ctrl+K` em qualquer tela para saltar entre as abas (`1`-`6`) ou executar comandos rápidos sem tirar as mãos do teclado.
6. **Painel Direito de Prévia e Inspetor:** Acompanhe o quadro ao vivo com selo *ON AIR / READY / STANDBY* e botões de vidro *Standby* / *Blackout*, metadados do arquivo, status das saídas e precisão de corte não destrutivo (`00:00.000`).

## Reprodução, Cortes, Zoom e Apresentação de PDFs

- **Cortes Não Destrutivos:** Os marcadores de início (`trimStartMs`) e fim (`trimEndMs`) são aplicados diretamente no `libmpv` sem alterar nem recodificar o arquivo original. Se o tempo final for menor ou igual ao inicial, o limite final é desconsiderado.
- **Controle de Zoom e Pan:** Ajuste o zoom (`1.0x` até `8.0x`) e o deslocamento horizontal/vertical (`-0.5` a `+0.5`) em vídeos, imagens e documentos PDF sincronizados com a saída externa, com opção de reset instantâneo.
- **Documentos PDF Multipágina:** Avance, retorne ou salte diretamente para qualquer página renderizada nativamente via `Qt6::Pdf` na resolução do monitor de exibição.

## Música Ambiente e Horários das Reuniões

- A música ambiente opera em um pipeline exclusivo de áudio do `libmpv`, totalmente isolado do player principal de apresentações.
- Configure os dias e horários das reuniões de meio e fim de semana para que a música ambiente realize automaticamente um fade-out suave (padrão `5000 ms`) encerrando exatamente no horário de início da reunião. Pausar ou retomar manualmente assume prioridade na sessão atual.

## Importação de Mídias

- **Arrastar e Soltar:** Arraste vídeos (`MP4`, `MKV`, `WebM`), áudios (`MP3`, `AAC`, `OGG`, `FLAC`), imagens (`JPEG`, `PNG`, `WebP`) ou documentos `PDF` diretamente para a playlist.
- **Pacotes de Publicações e Discursos (`.jwpub` e `S-34 / S-34mp`):** Abra arquivos `.jwpub` nativamente com `minizip-ng` (com leitura SQLite interna de `contents` e validação contra Zip-Slip) e extraia seletivamente apenas as imagens, vídeos ou mídias do discurso público escolhido, sem arquivos temporários sobrando em disco.
- **Playlists do JW Library (`.jwlplaylist`):** Importe pacotes `.jwlplaylist` preservando a ordem do `TagMap`, marcadores, flag de repetição (`EndAction`), mídias/miniaturas embutidas e pontos de corte convertidos a partir dos ticks de 100 nanossegundos.
- **Sincronização Semanal Automática e Catálogo:** Escolha o idioma das mídias da congregação (`refreshLanguages`), navegue entre semanas (cálculo automático das edições de `MWB` e *Sentinela*), clique em **Sincronizar Semana** no cabeçalho da Reunião ou em **Buscar e Preencher Semana** em *Downloads* para baixar pacotes `.jwpub` e vídeos `MP4` deduplicados automaticamente para as seções da reunião, ou busque qualquer publicação (`sjjm`, `lff`, `mwb`, `w`) com cache em disco via `ETag` / `Last-Modified`, gravação em `.part`, verificação SHA-256 e controles de pausa, retomada, nova tentativa ou cancelamento.
- **Pastas Sincronizadas na Nuvem:** Configure ou detecte automaticamente pastas locais do Dropbox, OneDrive, Google Drive ou Nextcloud para importar mídias estáveis automaticamente após o debounce de `500 ms`.

## Apresentação em Segunda Tela

Quando um segundo monitor ou projetor está conectado, o KMedia o atribui automaticamente para exibição em tela cheia sem bordas (janela `"KMedia - Presentation"`), mantendo o console do operador na tela principal. O botão *Standby / Blackout* alterna instantaneamente entre a mídia ativa e uma tela preta ou imagem de espera da congregação sem perder o estado da fila.

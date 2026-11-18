# Guia de Configurações

Cada perfil de congregação no KMedia mantém seu próprio banco de dados e pasta de arquivos isolados.

## Perfis de Congregação

- Crie perfis separados para diferentes congregações ou grupos de idioma que compartilham o mesmo computador.
- Defina diretório de armazenamento, idioma preferido do catálogo e horários semanais das reuniões de meio e fim de semana.

## Exibição e Standby

- **Monitor de Apresentação:** Use a seleção automática da segunda tela ou escolha manualmente a saída desejada (com retorno automático caso o monitor escolhido seja desconectado).
- **Decodificação por Hardware:** Utiliza aceleração segura por GPU (`auto-safe`) no `libmpv` para manter o notebook frio e silencioso.
- **Imagem de Standby:** Defina uma imagem personalizada da congregação para exibição enquanto nenhuma mídia estiver ativa ou em `Standby / Blackout`.

## Áudio e Música Ambiente

- **Dispositivo de Saída:** Escolha um dispositivo específico pela lista do `libmpv` para evitar microfonia ou eco entre o som local e o Zoom.
- **Música Ambiente Programada:** Defina os dias, horários de início das reuniões e a duração do fade-out (padrão `5000 ms`) para que a música ambiente faça o fade-out suave e pare exatamente na hora.

## Downloads de Rede e Sincronização na Nuvem

- **Concorrência na Fila de Downloads:** Limite transferências simultâneas de catálogo e mídias (`1` a `6`, padrão `3`) com HTTP/2, cache condicional por `ETag` / `Last-Modified`, gravação atômica via `.part` e checagem SHA-256.
- **Pastas Sincronizadas Monitoradas:** Defina diretórios locais do Dropbox, OneDrive, Google Drive ou Nextcloud (`cloud/watched_folders`) monitorados recursivamente por eventos do sistema com estabilização de `500 ms` antes da importação.

## OBS Studio e Zoom

- **OBS WebSocket v5:** Configure host, porta (padrão `4455`) e senha opcional para acionar cenas automaticamente ao iniciar, pausar ou encerrar mídias.
- **Ponte Zoom:** Salve o ID e a senha da reunião no Zoom para abrir com um clique via `zoommtg://`, com opção de automatizar o atalho de compartilhamento ao iniciar a exibição.

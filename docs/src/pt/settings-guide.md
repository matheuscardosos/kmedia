# Guia de Configurações

Cada perfil de congregação no KMedia mantém seu próprio banco de dados e pasta de arquivos isolados.

## Perfis de Congregação

- Crie perfis separados para diferentes congregações ou grupos de idioma que compartilham o mesmo computador.
- Defina diretório de armazenamento, idioma preferido do catálogo e horários semanais das reuniões de meio e fim de semana.

## Exibição e Standby

- **Monitor de Apresentação:** Use a seleção automática da segunda tela ou escolha manualmente a saída desejada.
- **Imagem de Standby:** Defina uma imagem personalizada da congregação para exibição enquanto nenhuma mídia estiver ativa.

## Áudio e Música Ambiente

- **Dispositivo de Saída:** Escolha um dispositivo específico pela lista do `libmpv` para evitar microfonia ou eco entre o som local e o Zoom.
- **Música Ambiente Programada:** Defina os horários de início das reuniões para que a música ambiente faça um fade-out suave de 5 segundos e pare exatamente na hora.

## OBS Studio e Zoom

- **OBS WebSocket v5:** Configure host, porta (padrão `4455`) e senha opcional para acionar cenas automaticamente ao iniciar, pausar ou encerrar mídias.
- **Ponte Zoom:** Salve o ID e a senha da reunião no Zoom para abrir com um clique via `zoommtg://`, com opção de automatizar o atalho de compartilhamento ao iniciar a exibição.

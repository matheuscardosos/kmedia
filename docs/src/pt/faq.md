# Perguntas Frequentes

## Por que o KMedia foi escrito em C++20 e Qt 6 em vez de Electron?

Aplicativos Electron empacotam um navegador Chromium inteiro e um processo Node.js, consumindo de 300 a mais de 800 MB de RAM e causando picos de CPU e ruído de ventoinha em notebooks simples. O KMedia compila para código nativo com Qt Quick e `libmpv`, visando menos de 45 MB de RAM em repouso e decodificação por GPU.

## Posso gerenciar mais de uma congregação no mesmo computador?

Sim. Cada perfil possui seu próprio banco SQLite WAL, diretório de mídia, agenda e ajustes de OBS e Zoom. A troca de perfil atualiza o console instantaneamente sem reiniciar o aplicativo.

## O KMedia recodifica vídeos ao definir cortes de início e fim?

Não. Os tempos de início e fim são salvos como metadados e aplicados diretamente pelo `libmpv` na reprodução, mantendo a qualidade original sem gastar CPU ou disco.

## Como reportar um erro ou sugerir uma melhoria?

Abra uma issue na [página de Issues do GitHub](https://github.com/matheuscardosos/kmedia/issues) informando sistema operacional, versão do KMedia e passos para reproduzir.

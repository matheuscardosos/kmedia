# Perguntas Frequentes

## Geral

### De onde o KMedia tira as mídias das reuniões?

Direto do site oficial das Testemunhas de Jeová e da sua rede de distribuição de conteúdo, as mesmas publicações e arquivos que o JW Library usa. O KMedia descobre o que cada reunião precisa e baixa, conferindo cada arquivo com o checksum publicado junto com ele. Ele não usa outros sites, pacotes ou curadores. O código-fonte é aberto, então qualquer pessoa pode conferir o que ele faz.

### O KMedia respeita os Termos de Uso do jw.org?

Sim. Os [Termos de Uso](https://www.jw.org/finder?docid=1011511&prefer=content&wtlocale=T) do jw.org permitem aplicativos gratuitos e sem fins comerciais que baixam arquivos eletrônicos como EPUB, PDF, MP3 e MP4 das áreas públicas do site. O KMedia é gratuito e sem fins comerciais, e só baixa arquivos desse tipo das áreas públicas.

### Em quais sistemas o KMedia funciona?

Windows 10 ou mais recente (64 bits), macOS em Apple silicon e Linux x64. Baixe o pacote certo na [página de download](./download.md).

### O KMedia funciona no meu idioma?

As mídias das reuniões vêm em qualquer idioma publicado no jw.org, com um idioma de reserva para o que faltar. A interface está em inglês e português, e é definida separada do idioma das mídias, então os dois podem ser diferentes. Congregações de língua de sinais recebem os cânticos do cancioneiro em língua de sinais automaticamente.

## Mídias da reunião

### Como o KMedia sabe o que cada reunião precisa?

Para a reunião de meio de semana ele lê a apostila, e para a de fim de semana encontra o artigo de estudo de *A Sentinela* daquela semana. Imagens, vídeos e cânticos vão para as suas seções na ordem dos parágrafos. O [Guia do Usuário](./user-guide.md#como-as-reunioes-sincronizam) explica como a sincronização funciona.

### Com quanta antecedência o KMedia prepara as reuniões?

Cinco semanas, começando pela atual, a reunião mais próxima primeiro. Cada nova semana entra conforme o calendário avança. Os dias com reunião ou com mídias que você adicionou ficam marcados no calendário da página *Reunião*.

### Como adiciono minhas próprias mídias?

Use **Adicionar mídia** na página *Reunião* ou arraste arquivos para a janela. O KMedia aceita vídeos, áudios, imagens, documentos PDF, publicações `.jwpub` e listas `.jwlplaylist` do JW Library. Cânticos, vídeos do JW.org, mídias de publicação e mídias do discurso público têm seus próprios seletores no mesmo menu.

### Cortar um vídeo muda o arquivo?

Não. O início e o fim que você define ficam guardados à parte e são aplicados durante a reprodução, então o original mantém a qualidade e nada é convertido.

## Apresentação

### Como apresento numa segunda tela ou projetor?

Conecte antes ou depois de abrir o KMedia. Ele apresenta nela em tela cheia e deixa os controles na tela principal. Escolha outra tela em *Configurações > Tela* e mostre ou esconda a janela de apresentação com `Ctrl+P`.

### Quais atalhos de teclado existem?

O [Guia do Usuário](./user-guide.md#atalhos-de-teclado) lista todos. Os principais são `Espaço` para reproduzir ou pausar, `B` para a espera e `Alt+Esquerda` ou `Alt+Direita` para o item anterior ou o próximo.

## Integrações

### Como uso o KMedia com o Zoom?

Salve o ID da reunião em *Configurações > Integrações*. Depois entre por **Abrir Reunião Zoom** ali mesmo ou pelo botão do Zoom da barra em pílula, e compartilhe a tela de apresentação no Zoom como de costume.

### Como conecto o OBS Studio?

No OBS Studio, ative o servidor WebSocket em *Ferramentas > Configurações do servidor WebSocket*. No KMedia, informe o host e a porta em *Configurações > Integrações* e clique em **Salvar e Conectar**. O botão do OBS na barra em pílula passa a listar suas cenas.

### Como funciona a pasta compartilhada?

Escolha, em cada computador que apresenta, uma pasta que o Dropbox, OneDrive, Google Drive ou Nextcloud mantém sincronizada. As mídias colocadas nas pastas com data (`AAAA-MM-DD`) aparecem naquele dia em todos eles. O [Guia de Configurações](./settings-guide.md#pasta-compartilhada) explica onde cada arquivo entra.

### Para que serve a exportação de mídias?

Para um computador ou reprodutor que só precisa dos arquivos. O KMedia copia cada dia com mídias para uma pasta com a data, na ordem da reunião e com nomes numerados, e mantém a cópia atualizada. Veja o [Guia de Configurações](./settings-guide.md#exportacao-de-midias).

### Posso usar várias congregações no mesmo computador?

Sim. Cada perfil de congregação tem suas próprias configurações, mídias, agenda e integrações. Troque pelo seletor de congregação no topo da navegação, sem reiniciar.

## Solução de problemas

### As mídias não baixam. O que devo conferir?

- Confira a conexão com a internet.
- Um ponto vermelho no calendário marca um dia com mídias que falharam. Abra esse dia e clique em **Tentar de novo**.
- O botão de downloads da barra em pílula mostra o que está em andamento e o que falhou.
- Se um item não existe no idioma das mídias, defina um **Idioma de reserva** em *Configurações > Reuniões*.

### O OBS Studio não conecta. O que devo conferir?

- O OBS Studio está aberto e o servidor WebSocket dele está ativo.
- O host e a porta em *Configurações > Integrações* são os mesmos do OBS Studio (`127.0.0.1` e `4455` no mesmo computador).
- Clique em **Salvar e Conectar** de novo depois de mudá-los.

### As mídias da pasta compartilhada não aparecem. O que devo conferir?

- Os arquivos estão dentro de uma pasta com data (`AAAA-MM-DD`) direto na pasta compartilhada.
- O aplicativo da nuvem terminou de sincronizar neste computador.
- A pasta compartilhada não é também a pasta de exportação.

## Perguntas técnicas

### Por que o KMedia foi escrito em C++ e Qt em vez de Electron?

Aplicativos feitos com Electron carregam um navegador inteiro junto. O KMedia é um aplicativo nativo feito com Qt e libmpv, então abre rápido, usa bem menos memória e reproduz vídeo pela placa de vídeo, o que mantém silenciosos e frios os notebooks mais antigos do Salão do Reino.

### O KMedia coleta dados sobre mim?

Não. O KMedia não tem telemetria e não envia relatórios. Ele só se conecta aos servidores do jw.org, para baixar mídias e procurar congregações, e ao OBS Studio e ao Zoom no seu próprio computador quando você os usa. Suas configurações e mídias ficam no seu computador.

### O KMedia se atualiza sozinho?

Ainda não. As novas versões são publicadas na [página de download](./download.md) e no GitHub Releases.

## Suporte

### Como relato um problema ou peço uma funcionalidade?

Abra uma issue no [GitHub](https://github.com/matheuscardosos/kmedia/issues). Para um problema, inclua a linha de diagnóstico de *Configurações > Segurança* e os passos que levam a ele.

### Como posso ajudar?

Código, traduções e documentação são bem-vindos. O [CONTRIBUTING.md](https://github.com/matheuscardosos/kmedia/blob/main/CONTRIBUTING.md) explica como compilar o KMedia e enviar mudanças.

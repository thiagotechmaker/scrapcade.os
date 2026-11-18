# Roteiro do ScrapCade OS

O que já entrou está no [CHANGELOG](../CHANGELOG.md). Aqui fica o que vem pela frente, na ordem combinada. Cada versão só é publicada depois de passar na bateria automática (`tests/testar.sh`).

## v0.23.0-beta (próxima)

- **ScreenScraper** opcional, com a conta do usuário (descrições, logos, vídeos). A API pede uma credencial de desenvolvedor do projeto, que ainda falta pedir ao ScreenScraper.
- **Teste de cada console** com jogos de verdade (lista dos mais pesados por console no guia de testes), e ajuste dos que ficarem lentos.
- **Fliperama do iChroma:** importar de um pen drive de verdade, analógico direito e teclado/mouse no Half-Life (testes que pedem alguém do lado do aparelho); sons do menu, se o som da placa voltar.

## Depois

- Xbox 360 (Xenia), quando rodar jogos no Linux.
- Tela vertical, pistola de luz, spinner e trackball, TV de tubo em 240p, Moonlight.
- Fluidez: boot em até 15 segundos, abrir e sair dos jogos mais rápido.
- Teste em mais hardware de verdade (notebooks e PCs de 32 bits).
- Compilação na nuvem (GitHub Actions) e imagem otimizada para processadores de 2015 em diante (x86-64-v3).

# Visualizador de Cenas 3D — Computação Gráfica (Unisinos)

Visualizador 3D com OpenGL moderna (3.3 core) que integra o pipeline desenvolvido ao longo do
semestre: leitura de malhas `.OBJ`/`.MTL`, mapeamento de texturas, iluminação de Phong com a
técnica de três pontos, câmera em primeira pessoa, seleção e transformação de objetos e animação
de trajetórias por curvas de Bézier. A cena é montada a partir de um arquivo de configuração
(`cena.txt`).

## Funcionalidades

- Leitura de múltiplos `.OBJ` triangularizados, com normais e coordenadas de textura.
- Materiais lidos do `.MTL` (Ka, Kd, Ks, Ns e `map_Kd`) usados no shader de iluminação.
- Iluminação de Phong (ambiente, difusa com atenuação e especular) com 3 luzes pontuais
  posicionadas automaticamente a partir do objeto principal (key, fill e back light).
- Câmera em primeira pessoa (WASD + mouse) com frustrum configurável.
- Seleção de objetos e aplicação de translação, rotação e escala uniforme.
- Trajetórias cíclicas por curva de Bézier (algoritmo de de Casteljau).
- Arquivo de configuração de cena em texto, com gravação do estado atual em tempo de execução.

## Setup e compilação

Dependências (baixadas automaticamente via CMake `FetchContent`): GLFW, GLM e stb.
A GLAD já está incluída em `include/glad` e `common/glad.c`.

```bash
cmake -S . -B build
cmake --build build
```

O executável `CG` é gerado em `build/`. Execute a partir da raiz do projeto (os caminhos do
`cena.txt` e dos assets são relativos a ela):

```bash
./build/CG
```

## Controles

| Ação | Tecla |
|------|-------|
| Mover câmera | `W` `A` `S` `D` |
| Subir / descer câmera | `SPACE` / `SHIFT` |
| Orientar câmera | mouse |
| Selecionar próximo objeto | `TAB` |
| Transladar objeto (X / Z) | setas |
| Transladar objeto (Y) | `PAGE UP` / `PAGE DOWN` |
| Rotacionar objeto (liga/desliga por eixo) | `X` `Y` `Z` |
| Escala uniforme | `[` / `]` |
| Ligar/desligar luzes | `1` `2` `3` |
| Adicionar ponto de controle (Bézier) | `B` |
| Iniciar/pausar animação | `N` |
| Limpar trajetória | `C` |
| Gravar a cena em `cena.txt` | `G` |
| Encerrar | `ESC` |

## Arquivo de configuração (`cena.txt`)

Cada linha define uma diretiva (`#` inicia comentário):

```
camera  px py pz  yaw pitch  fov near far
luz     indice  intensidade  ligada(0/1)
chao    n  tamanhoBloco     # tabuleiro n x n de grama com borda de pedra
objeto  caminho.obj  px py pz  ex ey ez anguloGraus  escala
traj    px py pz          # ponto de controle do último objeto declarado
veltraj velocidade        # velocidade da trajetória do último objeto
```

## Assets

Os modelos (`Suzanne.obj`, `SuzanneSubdiv1.obj`, `Cube.obj`) e texturas estão em
`assets/Modelos3D`, provenientes do repositório de exemplos da disciplina:
https://github.com/guilhermechagaskurtz/CGCCHibrido/tree/main/assets/Modelos3D
Exportados do Blender (`.obj`/`.mtl`). Verifique sempre a licença de uso antes de redistribuir.

## Referências

- LearnOpenGL — https://learnopengl.com
- Documentação OpenGL — https://docs.gl
- GLFW Input Guide — https://www.glfw.org/docs/latest/input_guide.html
- GLM — https://github.com/g-truc/glm
- Material e exemplos da disciplina — https://github.com/guilhermechagaskurtz/CGCCHibrido

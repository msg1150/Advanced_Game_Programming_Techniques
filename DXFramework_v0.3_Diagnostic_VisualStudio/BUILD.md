# DX Framework v0.3 Diagnostic

## 목적

v0.2에서 초기화 실패 팝업이 발생했을 때
어느 단계에서 실패했는지 확인하기 위한 진단 버전입니다.

## 실행

1. `DXFramework.sln` 실행
2. `Debug | x64`
3. `F5`

초기화가 실패하면 기존의 단순한 메시지 대신 다음과 같이 표시됩니다.

```text
초기화 실패 단계: Shader 초기화

검색한 Shader 폴더:
...\bin\x64\Debug\Shaders\Basic

HLSL Compile 실패
File: ...
Entry: VSMain
Target: vs_5_0
HRESULT: ...

Compiler Message:
...
```

## 확인 가능한 초기화 단계

- Window 초기화
- Renderer 초기화
- Shader 초기화
- Constant Buffer 초기화
- Cube Mesh 초기화

Shader 단계에서는 추가로 다음을 구분합니다.

- Shader 파일 없음
- Vertex Shader Compile 실패
- Pixel Shader Compile 실패
- CreateVertexShader 실패
- CreatePixelShader 실패
- CreateInputLayout 실패

Visual Studio의 `출력(Output) > 디버그(Debug)` 창에도 같은 초기화 실패 메시지가 기록됩니다.

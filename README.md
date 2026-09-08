# 3DGVRT (3D Gaussian Vulkan Ray Tracing)
Vulkan API를 기반으로 구현된 PC 및 모바일 크로스 플랫폼용 3D 가우시안 레이트레이서입니다.
[SaschaWillems/Vulkan](https://github.com/SaschaWillems/Vulkan)를 베이스라인으로 [3DGRT](https://github.com/nv-tlabs/3DGRUT)의 3D 가우시안 레이트레이싱 파이프라인을 Vulkan으로 구현했습니다.
나아가 undersampling 기법을 적용하여 품질 저하는 최소화하면서 렌더링 성능을 확보하기 위한 연구를 진행했습니다.

## Contributions

* **3D 가우시안 레이트레이싱 셰이더 작성:**  
3DGRT의 3D 가우시안 레이트레이싱 파이프라인을 바탕으로 Vulkan glsl로 포팅 및 최적화
* **베이스라인 코드 리팩토링:**  
다양한 샘플 프로젝트 구현을 위해 설계된(Monolithic) 구조를 개선하기 위해, 렌더링 파이프라인(Pipeline)과 디버깅 매니저(Debugging Manager)를 독립적인 클래스로 분리하여 코드의 확장성과 유지보수성을 확보함
* **Undersampling 기반 성능 최적화:**  
모바일 등 제한된 하드웨어 환경에서의 렌더링 성능 확보를 위해 픽셀 언더샘플링 기법을 파이프라인에 통합 적용하여, 시각적 품질 저하를 억제하면서 프레임레이트를 향상시킴.

## Tech Stack

- **Language:** C/C++
- **GPU API:** Vulkan
- **Key Concepts:** Realtime Ray Tracing, 3D Gaussian Splatting, Undersampling
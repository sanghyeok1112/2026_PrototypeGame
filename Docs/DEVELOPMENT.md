# 개발 및 검증 안내

## 주요 코드

| 코드 | 역할 |
|---|---|
| MyProjectPrototypeCharacter | 플레이어 이동, 조준·카메라, 무기 교체, 체력 |
| WeaponBase / GunWeapon / MeleeWeapon | 공통 공격 인터페이스, 원거리·근접 판정 |
| MonsterCharacter | 몬스터 체력·사망·근접 공격 단계 |
| MonsterAIController | 시야, 마지막 위치 조사, 플레이어 우선 및 방어 목표 이동 |
| BTTask_MonsterAttack / MonsterBTNodes | 행동 트리의 공격·이동 |
| ColonyCore | 광산 앞 임시 방어 목표와 체력 |
| DefensePrototypeManager | 적 생성, 웨이브, 승패, 숫자 표시, R키 재시작 |
| DefensePrototypeEditor / PrototypeMapEditor | 에디터 전용 배치 도구 |

`DefensePrototypeWidget`은 이전 패널 UI 코드입니다. 현재 매니저에서는 생성하지 않으며 숫자 디버그 텍스트를 사용합니다.

## 밸런스 조정

`Lvl_ApprovedLayout`의 다음 액터를 선택해 Details에서 조정합니다.

- `Defense_WaveManager`: Wave Sizes, Preparation Seconds, Intermission Seconds, Spawn Interval, Monster Class.
- `Defense_ColonyCore`: Max Health.
- `Defense_Spawn_Front`, `Defense_Spawn_CrashSide`: NavMesh 위의 생성 위치.

두 스폰 지점 모두에서 5회 연속 생성에 실패하면 설정 확인 메시지로 종료합니다.
매니저가 추적하는 웨이브 적만 처치 수에 반영합니다.

## 자동 검사

언리얼 Automation에서 다음 검사를 실행합니다.

- `Prototype.MonsterCombat.DamageEvadeWallAndDeath`
- `Prototype.ColonyDefense.CoreAIWavesAndResults`

코어 피격·플레이어 오발 무효·타격 전 피해 없음·실제 BT 이동 후 코어 공격,
생성 예정 적이 있을 때 조기 웨이브 완료 방지·3웨이브 승리·코어 파괴 패배,
플레이어 우선 공격·플레이어 사망 패배·결과 후 생성 중단 등을 검증합니다.

2026-11-03 검사에서는 위 두 테스트가 통과했습니다. 숫자 UI 및 R키 재시작 전환 후 Development Editor 빌드도 성공했습니다.
실제 맵의 정문·측면 스폰에서 코어까지 완전한 NavMesh 경로 2곳도 확인했습니다.
수동 UI 확인은 아직 완료하지 않았습니다.

## 에디터 도구 사용 주의

에디터 콘솔의 `PrototypeMap.*`, `MonsterAI.Setup`, `MonsterCombat.Setup`, `DefensePrototype.Setup` 명령은
프로토타입 제작용입니다. 저장된 맵을 플레이하기 위해 다시 실행할 필요는 없습니다.
다른 배치안을 생성하거나 에셋을 수정할 수 있으므로 제작 도구를 다시 실행하기 전에는 작업을 커밋하세요.

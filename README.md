# OwenGameServer

خادم ألعاب فورتنايت يعمل داخل عملية اللعبة على أندرويد ARM64 لإصدار **21.30** (com.epicgames.fortnite).

يقوم بتحويل نسخة العميل إلى خادم كامل: لاعبون متعددون، بقاء، بناء، غنائم، طائرة، عاصفة، نتائج مباريات، وتكامل مع باكند Lawin المحلي.

## المكونات

- `src/core` ربط SDK بذاكرة العملية، قراءة GObjects/GNames، تحويل UTF-16، SpawnActor، وأدوات FastArraySerializer
- `src/modules` منطق الخادم: GameMode, Player, Building, Inventory, Looting, Abilities, Misc, Creative, AC, API, Results, Tournaments, XP, Lategame
- `include/SDK` SDK إصدار 21.30 لالاندرويد (MobileDumper-7) وكل نداءات الدوال تمر عبر ProcessEvent
- `external` Dobby للربط و libcurl للاتصال بالباكند

## التهيئة

`src/options.h`

| الخيار | الوظيفة |
|---|---|
| bDuos | وضع الثنائي مع DBNO |
| bLateGame | بدء المباراة من العاصفة مع عتاد متكامل |
| bCreative | خريطة الكرياتف |
| bGameSessions | ربط matchmaking مع الباكند |
| bDev | وضع التطوير: يعطل AC والنتائج |
| BackendUrl | عنوان باكند Lawin |
| g_Port | منفذ الاستماع |

## البناء

GitHub Actions يبني `libgameserver.so` لـ arm64-v8a ويرفعه كـ artifact.

يتطلب تحميل المكتبة داخل عملية اللعبة عبر Owen-Launcher.

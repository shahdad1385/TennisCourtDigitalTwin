# Clay Court Tennis — UE 5.5.4

شبیه‌سازی تنیس خاکی با فیزیک سفارشی توپ، امتیازدهی ITF و کورت ساخته‌شده به‌صورت PBR.

## پیش‌نیازها

| مورد | نسخه |
|---|---|
| Unreal Engine | 5.5.4 در `C:\UE_5.5` |
| Visual Studio Build Tools 2022 | 17.14 + `MSVC v143` (14.44) + `Windows 11 SDK` |
| .NET Framework Developer Pack | 4.8.1 (برای ماژول `SwarmInterface` در UBT) |
| GPU | DX12 / SM6 (تست‌شده روی RTX 3070 Laptop) |

> هشدار `Visual Studio 2022 compiler version 14.44.35230 is not a preferred version` طبیعی است و بیلد را متوقف نمی‌کند.

## بیلد

```powershell
& 'C:\UE_5.5\Engine\Build\BatchFiles\Build.bat' ClayTennisEditor Win64 Development `
  -Project='C:\Projects\ClayTennis\ClayTennis.uproject' -WaitMutex
```

یا اسکریپت آماده:

```powershell
powershell -ExecutionPolicy Bypass -File C:\Projects\ClayTennis\Tools\build.ps1
```

خروجی: `Binaries\Win64\UnrealEditor-ClayTennis.dll`

## اجرا

- **ادیتور:** دابل‌کلیک `ClayTennis.uproject` (یا از Unreal Project Browser) → لِوِل `Content/Maps/TennisCourt` خودکار باز می‌شود → **Play**.
- **بازی مستقل (بدون ادیتور):**

```powershell
& 'C:\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe' `
  'C:\Projects\ClayTennis\ClayTennis.uproject' -game -windowed -ResX=1920 -ResY=1080
```

> اولین اجرا روی این ماشین به‌دلیل کامپایل شیدرها (Lumen + TSR + ابر حجمی) طولانی است؛ بار بعد از DDC خوانده می‌شود و سریع بالا می‌آید.

## کنترل‌ها

| کلید | کار |
|---|---|
| `A` / `D` | حرکت روی خط پایه |
| `W` / `S` | نزدیک/دور شدن از تور |
| ماوس | نشانه‌گیری (هدف ضربه) |
| چرخ ماوس | زوم دوربین |
| کلیک چپ (نگه‌داشتن) | شارژ قدرت ضربه، رها کردن = ضربه |
| `E` / `Q` | تغییر اسپین (تاپ‌اسپین / اسلایس / فلت) |
| `Space` | سرویس (وقتی نوبت سرویس توست) |
| `R` | ریست رالی |

## ساختار پروژه

```
ClayTennis.uproject
Config/            DefaultEngine.ini (Lumen, SM6, PostFX defaults), DefaultGame.ini, DefaultInput.ini
Source/ClayTennis/ 20+ فایل C++ (فیزیک، AI، امتیازدهی، HUD، کورت، راکت، بازیکن)
Content/
  Maps/TennisCourt.umap    صحنه کامل (کورت، سکو، نورافکن، نورپردازی)
  Materials/               ۱۱ مستر + اینستنس‌ها (خاک رس، خط، تور، توپ، ...)
  Textures/                ۲۷ تکسچر (۴K واقعی + پروسیجرال)
Tools/             اسکریپت‌های ساخت (Python / PowerShell)
RawAssets/         سورس تکسچرها، لاگ بیلدها، رفرنس‌ها
Docs/              گزارش ماژول C++
```

## کیفیت و فیزیک

- **رندر:** Lumen (GI + reflections)، Virtual Shadow Maps، TSR، Nanite، Volumetric Clouds/Fog، PostFX سینمایی (Bloom, DOF, CA, Grain, Grade)
- **فیزیک توپ:** ۵۷ گرم / ۶۷ میلی‌متر، RK4 با گام ۲۴۰Hz، درگ `Cd = 0.55 + 0.07S`، مگنوس `Cl ≤ 0.45`، اسپین تا ۴۰۰۰rpm، بونس خاک رس `e=0.78`, `μ=0.55`
- **ابعاد:** ITF کامل (۲۳.۷۷ × ۱۰.۹۷ متر، سرویس ۶.۴۰ متر، تور ۰.۹۱۴/۱.۰۷ متر)
- **امتیازدهی:** ۱۵/۳۰/۴۰، دوس/ادونتیج، ست ۶ گیم با اختلاف ۲، تای‌بریک ۷، بست‌آو-۳/۵

## محدودیت‌های شناخته‌شده

- صدا (برخورد توپ/تماشاگر) پیاده‌سازی نشده؛ می‌توان صداهای CC0 اضافه کرد.
- دنباله‌ی توپ (trail) و افکت گرد و خاک بونس فقط به‌صورت مارک بونس روی زمین است.
- Crowd انیمیشن ندارد (تکسچر ثابت).
- بسته‌بندی Shipping تست نشده؛ دستور آماده است (`RunUAT BuildCookRun`).
- کاراکترها از استاتیک‌مش‌های Engine ساخته شده‌اند (بدون اسکلت/انیمیشن).

## اعتبار دارایی‌ها

- `sandy_gravel` (Diffuse/Rough/Normal/AO) از [Polyhaven](https://polyhaven.com) — لایسنس CC0
- سایر تکسچرها (خاک رس، تور، توپ، خط، جمعیت، لوگو، راکت) به‌صورت پروسیجرال در `Tools/gen_textures.ps1` تولید شده‌اند (seed ثابت ۱۳۳۷، خروجی تکرارپذیر با SHA-256).

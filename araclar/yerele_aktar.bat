@echo off
chcp 65001 >nul
REM ZEHRA KINIK - Bulut Claude Code oturumunu bu bilgisayara aktarir.
REM Yapar: Git/Node kontrolu -> Claude Code kurulumu -> projeyi indirir -> dali secer -> oturumu ceker.
setlocal
set REPO=https://github.com/oduncugur/orrrrrospu-zehra-k-n-k.git
set DAL=claude/tender-fermat-7w0r3w
set KLASOR=%USERPROFILE%\ZehraKinik

where git >nul 2>&1 || (
  echo [!] Git yok. Kuruluyor ^(winget^)...
  winget install -e --id Git.Git --accept-package-agreements --accept-source-agreements || goto hata
  echo [i] Git kuruldu. Bu pencereyi kapatip betigi TEKRAR calistirin.
  pause & exit /b
)
where node >nul 2>&1 || (
  echo [!] Node.js yok. Kuruluyor ^(winget^)...
  winget install -e --id OpenJS.NodeJS.LTS --accept-package-agreements --accept-source-agreements || goto hata
  echo [i] Node.js kuruldu. Bu pencereyi kapatip betigi TEKRAR calistirin.
  pause & exit /b
)
where claude >nul 2>&1 || (
  echo [i] Claude Code kuruluyor...
  call npm install -g @anthropic-ai/claude-code || goto hata
)

if exist "%KLASOR%\.git" (
  echo [i] Proje zaten var, guncelleniyor: %KLASOR%
  cd /d "%KLASOR%" && git fetch origin %DAL% && git checkout %DAL% && git pull origin %DAL% || goto hata
) else (
  echo [i] Proje indiriliyor: %KLASOR%
  git clone %REPO% "%KLASOR%" || goto hata
  cd /d "%KLASOR%" && git checkout %DAL% || goto hata
)

echo.
echo [i] Oturum cekiliyor. Tarayici acilirsa Claude hesabinla giris yap,
echo     listeden "ZEHRA KINIK" oturumunu sec.
echo.
call claude --teleport
goto son

:hata
echo.
echo [X] Bir adim basarisiz oldu. Yukaridaki hata mesajini kopyalayip Claude'a gonder.
:son
pause

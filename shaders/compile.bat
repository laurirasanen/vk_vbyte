@echo off

cd /d %~dp0

echo Compiling shaders
for %%i in (*.comp) do "%VULKAN_SDK%/Bin/glslangValidator.exe" -V "%%~i" -o "%%~i".spv

exit 0
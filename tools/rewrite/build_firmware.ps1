param([string]$Toolchain='D:\Tools\embedded\arm-gcc-14.2\bin',[switch]$WaveTest,[switch]$Ad9833)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$sourceRoot=Join-Path $projectRoot 'firmware_next'
$outputDirectory=Join-Path $sourceRoot 'build-target'
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$compiler=Join-Path $Toolchain 'arm-none-eabi-gcc.exe'
try {
    & $compiler --version | Select-Object -First 1
    if($LASTEXITCODE -ne 0){throw 'Compiler cannot start'}
} catch { throw "GNU toolchain could not start: $($_.Exception.Message). Supply a working -Toolchain directory; no image was built." }
$flags=@('-mcpu=cortex-m4','-mthumb','-mfpu=fpv4-sp-d16','-mfloat-abi=hard','-std=c11','-O2','-g3',
    '-ffunction-sections','-fdata-sections','-fstack-usage','-Wall','-Wextra','-Werror','-DSTM32F407xx',
    "-I$(Join-Path $sourceRoot 'include')",
    "-I$(Join-Path $projectRoot 'firmware\Drivers\CMSIS\Include')",
    "-I$(Join-Path $projectRoot 'firmware\Drivers\CMSIS\Device\ST\STM32F4xx\Include')")
if($WaveTest){
    $flags+=@('-DLCR_WAVE_TEST=1')
}
if($Ad9833){
    $flags+=@('-DLCR_EXCITATION_DAC=0u')
}
$name=if($WaveTest){'lcr-wave'}elseif($Ad9833){'lcr-ad9833'}else{'lcr-next'}
$objects=@()
foreach($directory in @('src','platform')) {
    foreach($source in Get-ChildItem (Join-Path $sourceRoot $directory) -Filter '*.c' -File) {
        $object=Join-Path $outputDirectory "$directory-$($source.BaseName).o"
        & $compiler @flags -c $source.FullName -o $object
        if($LASTEXITCODE -ne 0){throw "Compile failed: $($source.Name)"}
        $objects+=$object
    }
}
$elf=Join-Path $outputDirectory "$name.elf"
$map=Join-Path $outputDirectory "$name.map"
& $compiler @flags @objects -nostartfiles '-specs=nano.specs' '-specs=nosys.specs' `
    "-T$(Join-Path $sourceRoot 'STM32F407VE.ld')" '-Wl,--gc-sections' "-Wl,-Map=$map" '-Wl,--start-group' -lc -lm '-Wl,--end-group' -o $elf
if($LASTEXITCODE -ne 0){throw 'Firmware link failed'}
foreach($format in @(@('ihex','hex'),@('binary','bin'))) {
    & (Join-Path $Toolchain 'arm-none-eabi-objcopy.exe') -O $format[0] $elf (Join-Path $outputDirectory "$name.$($format[1])")
    if($LASTEXITCODE -ne 0){throw 'Image conversion failed'}
}
& (Join-Path $Toolchain 'arm-none-eabi-size.exe') $elf
if($LASTEXITCODE -ne 0){throw 'Image size verification failed'}
Write-Host "Firmware images: $outputDirectory"

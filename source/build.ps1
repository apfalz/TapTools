$env:CMAKE = $env:CMAKE_PATH + "\bin\cmake.exe"
if ( $env:PLATFORM -eq "x64" ) {
  $env:CMAKE_GENERATOR = "Visual Studio 12 2013 Win64"
  $env:CUSTOM_FLAG = "-DWIN64:Bool=True"
} else {
  $env:CMAKE_GENERATOR = "Visual Studio 12 2013"
  $env:CUSTOM_FLAG = ""
}

mkdir build
cd build

$env:CMAKE -G $env:CMAKE_GENERATOR $env:CUSTOM_FLAG -DCMAKE_BUILD_TYPE=Release .. > c:\projects\TapTools\configure.log
$env:CMAKE --build . --config Release > c:\projects\TapTools\build.log

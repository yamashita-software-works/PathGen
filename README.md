# PathGen


PathGenは指定されたファイルのパスを指定された形式で出力します。


Usage:


    pathgen [PrintTypeSwitch][Options] file1 file2 ...

Print type switch:

These switches are exclusive.

```
/drive
/dos
/dosdrive     Displays by MS-DOS drive path.
              d:\<Path>
/nt
/ntdevice     Displays by NT device name.
              \Device\HarddiskVolumeN\<Path>

/ntdrive      Displays by NT dos drive.
              \??\d:\<Path>

/ntguid       Displays by NT dos device guid.
              \??\Volume{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}\<Path>

/ntdosdevice  Displays by NT dos device name.
              \??\HarddiskVolume1\<Path>

/win32drive   Displays by Win32 drive path.
              \\?\d:\<Path>

/win32guid
/guid         Displays by Win32 guid path.
              \\?\Volume{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}\<Path>

/root         Displays root relative path (without volume name).

/dump         Print a specified file path with available all prefix.
```

Options:

```
/s            Use short path name.

/l            Use long path name.

/x            Replace path separator character backslash to slash.

/r            Make paths every occurrence of the specified file name 
              within the specified directory and all subdirectories.
```

実行例:

```
C:\>pathgen /nt c:\windows\
\Device\HarddiskVolume5\windows\addins
\Device\HarddiskVolume5\windows\appcompat
\Device\HarddiskVolume5\windows\apppatch
\Device\HarddiskVolume5\windows\AppReadiness
\Device\HarddiskVolume5\windows\assembly
\Device\HarddiskVolume5\windows\ativpsrm.bin
\Device\HarddiskVolume5\windows\bcastdvr
\Device\HarddiskVolume5\windows\bfsvc.exe
...
```

```
C:\>pathgen /win32guid c:\windows\
\\?\Volume{b70b2d8f-0000-0000-0000-800659020000}\windows\addins
\\?\Volume{b70b2d8f-0000-0000-0000-800659020000}\windows\appcompat
\\?\Volume{b70b2d8f-0000-0000-0000-800659020000}\windows\apppatch
\\?\Volume{b70b2d8f-0000-0000-0000-800659020000}\windows\AppReadiness
\\?\Volume{b70b2d8f-0000-0000-0000-800659020000}\windows\assembly
\\?\Volume{b70b2d8f-0000-0000-0000-800659020000}\windows\ativpsrm.bin
\\?\Volume{b70b2d8f-0000-0000-0000-800659020000}\windows\bcastdvr
\\?\Volume{b70b2d8f-0000-0000-0000-800659020000}\windows\bfsvc.exe
...
```

## Build方法

### 開発環境
ソースからビルドするには　Windows Driver Kit Version 7.1.0 (WDK) と Windows SDK for Windows 7 and .NET Framework 4 (Veriosn 7.1)が必要です。

https://www.microsoft.com/en-us/download/details.aspx?id=11800

https://www.microsoft.com/en-us/download/details.aspx?id=8442

>ISOファイルを使ってSDKをインストールする場合、プラットフォームごとに異なるので注意してください。   
>64bit環境にインストールする時は GRMSDKX_EN_DVD.iso を、
>32bit環境にインストールする時は GRMSDK_EN_DVD.iso をダウンロードしてください。
>適合しないファイルを使用するとエラーとなりインストールできません。



現在のビルド環境は、上記WDKとSDKが以下の場所にインストールされている前提になっています。WDKはデフォルトで下記の場所になっていますが、SDKのデフォルトは異なっているので注意してください。

WDK   
`C:\WinDDK\7600.16385.1`

SDK   
`C:\WinSDK\7.1`

もし別の場所にインストールされている場合は、その場所へのリンクを持つ上記パスと同じ名前のジャンクションポイントをC:ドライブのルートに作成すると便利です。

例)
`C:\WinSDK\7.1 -> C:\Program Files\Microsoft SDKs\v7.1`

>**Warning**   
>現状、ビルドに使用するsourcesファイル内に記述されたWDK/SDKルートパスがハードコードされているためです。
>独自のインストール先を設定したい場合は、sourcesファイルを編集して当該パスを調整する必要があります。
>編集する場合、sourcesファイルに記述するパスにはスペースを含めないでください。

> **Note**   
>SDKのセットアップは、マウントされた(またはCD/DVD)ドライブのルートにあるsetup.exeではなく、Setupフォルダ下のSDKSetup.exe を実行してください。   
> `\Setup\SDKSetup.exe`
>
>
>もしインストール時にエラーが発生した場合は、以下のVS2010再頒布モジュールをアンインストールしてから再度試してみてください。
>
>`Microsoft Visial C++ 2010 x86 Redistributable - 10.0.xxxxx`   
>`Microsoft Visial C++ 2010 x64 Redistributable - 10.0.xxxxx`

> **Note**   
>SDKから最低限インストールが必要なものは、ヘッダファイルとライブラリファイルのみです。コンパイラやツールは不要です。


<br>

### ビルド方法
スタートメニューの以下の項目を開きます。

`Windows Driver Kits > WDK 7600.16385.1 > Build Environments>Windows 7`

から

64ビット版をビルドする場合は、`x64 Free Build Environment`

32ビット版をビルドする場合は、 `x86 Free Build Environment`

のどちらかを開きます。

> **Warning**   
Windows 10ではスタートメニュー(Windows Driver Kits)から適切な開発環境を選べない場合があります（シェルリンク名が同じであるため）。
正しく選択できない場合は、シェルリンクがあるスタートアップメニューのフォルダを開いて直接選択してください。

<br>
コマンドプロンプトが開くので、ソースの展開先ディレクトリへ移動して、以下のbuildコマンドを実行します。
<br>
<br>

    build -c

最初のビルドでは以下のオプションをお勧めします。

    build -c -M 1



## License

Copyright (C) YAMASHITA Katsuhiro. All rights reserved.

Licensed under the [MIT](LICENSE) License.

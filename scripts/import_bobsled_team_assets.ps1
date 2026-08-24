param(
    [string]$SourceRoot = 'D:\PVZ\中文年度加强版完整版\Test'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$resourceRoot = Join-Path $repoRoot 'build\clang-release\resources'

# 原版资源逐文件锁定；只要来源包发生变化就停止，避免把未知版本静默写进权威运行资产。
$assets = @(
    @{ Source = 'reanim\Zombie_bobsled.reanim'; Destination = 'reanim\Zombie_bobsled.reanim'; Hash = '501C4549E37D96CD8266206EAA30BE6802528B64F397DCD94C3F4191B9302B20' },
    @{ Source = 'reanim\Zombie_bobsled_body1.png'; Destination = 'image\reanim\Zombie_bobsled_body1.png'; Hash = 'D19DF8997D3ADFC9CB2D228F421209DA8E760EBBFD60390E3FF645DA0CCF6732' },
    @{ Source = 'reanim\Zombie_bobsled_body2.png'; Destination = 'image\reanim\Zombie_bobsled_body2.png'; Hash = '30413289F8476A7F6A277535668A7565CB278C369EEE51A994E8B6C24B63A4F5' },
    @{ Source = 'reanim\Zombie_bobsled_innerarm_hand.png'; Destination = 'image\reanim\Zombie_bobsled_innerarm_hand.png'; Hash = '99A6A5120B1D3D30886AF74243BB02438ECB1245C2C7CC50C9BAD260D2287362' },
    @{ Source = 'reanim\Zombie_bobsled_innerarm_hand2.png'; Destination = 'image\reanim\Zombie_bobsled_innerarm_hand2.png'; Hash = '1CBF7F0BC7767CBAD3245E340F1F1DBB52316603700B490FC59E4CA325502D01' },
    @{ Source = 'reanim\Zombie_bobsled_innerarm_lower.png'; Destination = 'image\reanim\Zombie_bobsled_innerarm_lower.png'; Hash = '5FC094EA24E592AC526DDA952A6054342C5DC12CF73F4B336082B46B858D8F5B' },
    @{ Source = 'reanim\Zombie_bobsled_innerarm_upper.png'; Destination = 'image\reanim\Zombie_bobsled_innerarm_upper.png'; Hash = '242D6FF4A00BF5E2BB5B960779266613D4E8DB039B8B7859D74FEE635A7ADC66' },
    @{ Source = 'reanim\Zombie_bobsled_innerleg_foot.png'; Destination = 'image\reanim\Zombie_bobsled_innerleg_foot.png'; Hash = 'F3450DF6C3A040163C68EEAEC7E446D4197B097FB35E28228FE3B6944DA7D951' },
    @{ Source = 'reanim\Zombie_bobsled_innerleg_lower.png'; Destination = 'image\reanim\Zombie_bobsled_innerleg_lower.png'; Hash = 'B7EAFBE1FBDE8D88CB7F8F9A35B2E1E52513A2C0DD8847F0664ED80778317ED9' },
    @{ Source = 'reanim\Zombie_bobsled_innerleg_upper.png'; Destination = 'image\reanim\Zombie_bobsled_innerleg_upper.png'; Hash = 'DFC380ECEE07AE8958A0C528DBF0ED4287E9896A6BADE23FBF1AC651AA338395' },
    @{ Source = 'reanim\Zombie_bobsled_newhead.png'; Destination = 'image\reanim\Zombie_bobsled_newhead.png'; Hash = 'F48D2825B41F153607EF4B334F9813981D8263E8B20E36306DDF11C51143D499' },
    @{ Source = 'reanim\Zombie_bobsled_outerarm_hand.png'; Destination = 'image\reanim\Zombie_bobsled_outerarm_hand.png'; Hash = '716DC4C333513BAE533069D30D30BB024D5BD3E83809FDD9CEBAE0FA4CDD8C92' },
    @{ Source = 'reanim\Zombie_bobsled_outerarm_hand2.png'; Destination = 'image\reanim\Zombie_bobsled_outerarm_hand2.png'; Hash = '1CDC66AC6304121A72587390143F300521D60809A7202F1E6B814FC42A894A5B' },
    @{ Source = 'reanim\Zombie_bobsled_outerarm_lower.png'; Destination = 'image\reanim\Zombie_bobsled_outerarm_lower.png'; Hash = 'FF6D2450BF7D75C205E2613DC99D0D53448E13A5745269ACF7915D382F18784A' },
    @{ Source = 'reanim\Zombie_bobsled_outerarm_upper.png'; Destination = 'image\reanim\Zombie_bobsled_outerarm_upper.png'; Hash = 'FC282B4257BC7545595CC54A80C2A97EF8CDDBDE42BE905BE4D6545026857B7B' },
    @{ Source = 'reanim\Zombie_bobsled_outerarm_upper2.png'; Destination = 'image\reanim\Zombie_bobsled_outerarm_upper2.png'; Hash = '917E7F2B8C28BEFC891B47D6B1AEC84185684A040E800F49AD089B8DBFBEB629' },
    @{ Source = 'reanim\Zombie_bobsled_outerleg_foot1.png'; Destination = 'image\reanim\Zombie_bobsled_outerleg_foot1.png'; Hash = '443F146A899E9036A43EB744CE9511574D54D560BF8AB755468FC876D04FEC88' },
    @{ Source = 'reanim\Zombie_bobsled_outerleg_foot2.png'; Destination = 'image\reanim\Zombie_bobsled_outerleg_foot2.png'; Hash = '2585C51C4C2705203368C072920FE2AE8C3C9431C8114788F8B94E5AD1880876' },
    @{ Source = 'reanim\Zombie_bobsled_outerleg_lower.png'; Destination = 'image\reanim\Zombie_bobsled_outerleg_lower.png'; Hash = '9039FF474379FA9BA37C2CB25CF4E20B3FE57C440BB69584698FC3E44F4AA4FD' },
    @{ Source = 'reanim\Zombie_bobsled_outerleg_upper.png'; Destination = 'image\reanim\Zombie_bobsled_outerleg_upper.png'; Hash = 'DD0BD21B12D45DE71006B94DAB47FDD340DADEC3A85713E0B48B817158412B72' },
    @{ Source = 'images\Zombie_bobsled_inside.png'; Destination = 'image\Zombie_bobsled_inside.png'; Hash = 'EE7103D89AB95EB10CC70AF62DCAE0B3032240D6641D54E56346B7D2A2A7CA44' },
    @{ Source = 'images\Zombie_bobsled1.png'; Destination = 'image\Zombie_bobsled1.png'; Hash = 'CD4B65398D9E57FD34907A04C351037D76B4FB4744B0A6FA0484A180D4A66F84' },
    @{ Source = 'images\Zombie_bobsled2.png'; Destination = 'image\Zombie_bobsled2.png'; Hash = '3EDAB35B0DA7D550B18EE989D666F3A9278BA5FAB1C5038F23D965A44C17B259' },
    @{ Source = 'images\Zombie_bobsled3.png'; Destination = 'image\Zombie_bobsled3.png'; Hash = '0463F545D60A1C2CB138B06C2B718F6672610C65E2C60996EFD18394193C0256' },
    @{ Source = 'images\Zombie_bobsled4.png'; Destination = 'image\Zombie_bobsled4.png'; Hash = 'A35FD099D2ABBEE14C5562FCCF430B77788FF8DFAC7A7AA78F33A1C99379815B' },
    @{ Source = 'particles\ZombieBobsledHead.png'; Destination = 'particles\ZombieBobsledHead.png'; Hash = '67095CA7FD102B94FA86F57301B9C70BC666EB595D4E0E44F37C857A5F0133A1' }
)

foreach ($asset in $assets) {
    $sourcePath = Join-Path $SourceRoot $asset.Source
    if (-not (Test-Path -LiteralPath $sourcePath)) {
        throw "Missing original bobsled asset: $sourcePath"
    }
    $sourceHash = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash
    if ($sourceHash -ne $asset.Hash) {
        throw "Original asset hash mismatch: $($asset.Source) expected=$($asset.Hash) actual=$sourceHash"
    }

    $destinationPath = Join-Path $resourceRoot $asset.Destination
    $destinationDirectory = Split-Path -Parent $destinationPath
    New-Item -ItemType Directory -Force -Path $destinationDirectory | Out-Null
    Copy-Item -LiteralPath $sourcePath -Destination $destinationPath -Force

    $destinationHash = (Get-FileHash -LiteralPath $destinationPath -Algorithm SHA256).Hash
    if ($destinationHash -ne $asset.Hash) {
        throw "Copied asset verification failed: $destinationPath"
    }
    Write-Output "$($asset.Destination) $destinationHash"
}

Write-Output "Imported and verified $($assets.Count) original bobsled assets."

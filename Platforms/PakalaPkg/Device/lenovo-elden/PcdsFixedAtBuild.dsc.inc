# Geometry of the framebuffer inherited from the stock bootloader.
[PcdsFixedAtBuild.common]
  gAndromedaPkgTokenSpaceGuid.PcdMipiFrameBufferWidth|1904
  gAndromedaPkgTokenSpaceGuid.PcdMipiFrameBufferHeight|3040
  gAndromedaPkgTokenSpaceGuid.PcdMipiFrameBufferPixelBpp|32

  # The EFI partition picker supplies its own three-second timeout.
  gEfiMdePkgTokenSpaceGuid.PcdPlatformBootTimeOut|0

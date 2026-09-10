# Standalone probe verified visible writes with this geometry on 1.1.11.263.
# Black/white output does not distinguish RGB/BGR channel ordering.
[PcdsFixedAtBuild.common]
  gAndromedaPkgTokenSpaceGuid.PcdMipiFrameBufferWidth|1904
  gAndromedaPkgTokenSpaceGuid.PcdMipiFrameBufferHeight|3040
  gAndromedaPkgTokenSpaceGuid.PcdMipiFrameBufferPixelBpp|32
  gEmbeddedTokenSpaceGuid.PcdPrePiProduceMemoryTypeInformationHob|FALSE

[BuildOptions.common]
  *_*_*_CC_FLAGS = -DALOHA_FB_FONT_SCALE=3 -DELDEN_FRAMEBUFFER_CONSOLE=1 -DELDEN_COMPACT_LOG=1

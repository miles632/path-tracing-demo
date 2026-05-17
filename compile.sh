GLSLC="/home/clearchus/1.4.321.1/x86_64/bin/glslangValidator"
OUTDIR="shaders"

$GLSLC -V -Iinclude --target-env vulkan1.2 -S rgen shaders/raytrace.rgen -o $OUTDIR/rgen.spv
$GLSLC -V -Iinclude --target-env vulkan1.2 -S rchit shaders/raytrace.rchit -o $OUTDIR/rchit.spv
$GLSLC -V -Iinclude --target-env vulkan1.2 -S rmiss shaders/raytrace.rmiss -o $OUTDIR/rmiss.spv
$GLSLC -V -Iinclude --target-env vulkan1.2 -S rmiss shaders/shadow.rmiss -o $OUTDIR/smiss.spv

#$GLSLC -V --target-env vulkan1.2 -S comp shaders/copyToSwapchain.comp -o $OUTDIR/copy.spv

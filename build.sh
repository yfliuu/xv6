make clean
make out/loader.elf && cp out/loader.elf ../run_acrn/vmfuncvm1.flat
cp out/kernel.asm callee.asm
make clean
mv README README_BAK
echo "THERE IS NOTHING IN THIS README FILE" > README
make out/loader.elf CALLEE=no && cp out/loader.elf ../run_acrn/vmfuncvm2.flat
rm README
mv README_BAK README

# Control4Free USB Direct v0.2 — prototipo de reconexión

Basado en Control4Free 1.2.0 (GPL-3.0) con la modificación USB Direct v0.1 que el usuario probó con éxito en PS4 13.02 durante al menos 12 minutos.

**NO es una compilación probada:** el paquete contiene un parche de código, no el ELF. La estabilidad de **v0.2** al desenchufar y enchufar depende de cómo la PS4 entregue eventos de desconexión a `libSceUsbd`.

## Cambios mínimos sobre la v0.1

- Mantiene el sistema de dispositivo virtual nativo y el mapeo Xbox 360 -> DualShock 4 sin cambios.
- El hilo USB utiliza `sceUsbdCheckConnected` para comprobar desconexión únicamente tras un intervalo sin datos; se requieren dos resultados consecutivos de desconexión. Si la función no está exportada, utiliza una heurística limitada basada en errores no-timeout (no garantiza recuperar todos los tipos de desenchufe).
- En desconexión, neutraliza inmediatamente todos los botones/sticks y cierra solo el handle USB (no cierra el servicio ni elimina el mando virtual asignado al usuario).
- Reintenta abrir el mando USB cada 750 ms después de detectar desconexión; cuando lo encuentra, vuelve a leer informes USB sin crear un segundo mando virtual.
- Evita la recreación de mandos virtuales nuevos mientras el USB figura desconectado. Conserva como mínimo un pad previamente creado y asignado en la consola; al iniciar v0.2 sigue siendo necesario enchufar Xbox **antes** de ejecutar el payload.
- El juego sigue sin efectuar ninguna espera/lectura USB. Las lecturas USB son exclusivas del hilo background, con el mismo polling de v0.1 (hasta 250 Hz, timeout 12 ms).
- Identidad del ejecutable separada: `control4free-usb-v0_2.elf`, no sobrescribe la v0.1.

## Requisitos previos

Ya tener funcionando en Ubuntu `~/ps4-dev/Control4Free_USB_Direct/` (v0.1), `PS4_PAYLOAD_SDK` y `llvm-config-18`.

## Crear un clon de trabajo (NO editar v0.1)

```bash
cd ~/ps4-dev
cp -a Control4Free_USB_Direct Control4Free_USB_Direct_v0_2
```

Copiar este ZIP a `~/ps4-dev`, descomprimir y ejecutar:

```bash
cd ~/ps4-dev
unzip Control4Free_USB_Direct_v0_2_patch.zip
python3 Control4Free_USB_Direct_v0_2/apply_usb_direct_v0_2.py ~/ps4-dev/Control4Free_USB_Direct_v0_2
```

## Construir en Ubuntu

```bash
SDK="$HOME/ps4-dev/payload-sdk-install/ps4-payload-sdk"
export PS4_PAYLOAD_SDK="$SDK"
export LLVM_CONFIG="$(command -v llvm-config-18)"
export PATH="$SDK/bin:$PATH"
cd ~/ps4-dev/Control4Free_USB_Direct_v0_2
make clean
make
ls -lh build/control4free-usb-v0_2.elf
```

El clon conserva la marca de versión original 1.2.0, pero el payload lleva el sufijo `usb-direct-v0.2` en su identidad de compilación y bitácora.

## Instalación/pruebas después de compilar y verificar

Con los hooks antiguos desactivados, detener Control4Free/USB Direct v0.1, dejar AutoRun apagado. Conservar sin modificar `/data/payloads/control4free-usb.elf` y `/data/payloads/control4free.elf`.
Copiar el nuevo `control4free-usb-v0_2.elf` a `/data/payloads/` y ejecutarlo **solo una vez** desde el PayLoader LaunchPad. Dejar el mando enchufado antes de iniciar.

Primero repetir la prueba estable sin tocar USB: selección de usuario, movimientos, FPS, P2 independiente. Solo después comprobar la reconexión en el menú de PS4, nunca durante una partida importante. Dejar el DualShock 4 disponible para parar/reiniciar si la reconexión falla. No habilitar AutoRun en v0.2.

Registros: `/data/control4free/control4free.log` (tener en cuenta que Control4Free rota los archivos al iniciar una instancia nueva). Ver las líneas:

```
USB direct v0.2: Xbox disconnected; inputs neutral, scanning for USB reconnect
USB direct v0.2: Xbox reconnected; original virtual pad retained
```

Si tras desenchufar el controlador no detecta la pérdida, los botones se neutralizan automáticamente a los 350 ms como en v0.1, pero puede no producirse la recuperación. No desenchufar dos dispositivos iguales al mismo tiempo; esta rama es exclusiva de un solo Xbox 360 USB VID:PID 045E:028E.

**Rollback:** detener v0.2; si no responde, reiniciar PS4 y reactivar GoldHEN; iniciar `control4free-usb.elf` v0.1. La compilación v0.1 original y el Control4Free oficial siguen intactos.

## Licencia y procedencia

Proyecto derivado de Control4Free de MoHadiShibli (GPL-3.0) e inspiración para protocolo del plugin Xbox Controller de crucifix86 (MIT). Mantener avisos de licencia y facilitar fuente correspondiente al compartir ejecutables.

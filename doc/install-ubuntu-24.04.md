# Compilar AzerothCore desde cero en Ubuntu 24.04

Guía completa para este repositorio: dependencias, MySQL 8.4 LTS, CMake,
datos del cliente, configuración y primer arranque.

> **Ubuntu 24.02 no existe.** La LTS actual es **Ubuntu 24.04** (Noble Numbat).
> Los comandos de esta guía están validados contra 24.04.

Documentación oficial de AzerothCore (inglés):
[wiki de instalación](https://www.azerothcore.org/wiki/installation).

---

## 0. Qué vas a montar

| Pieza | Valor por defecto |
| --- | --- |
| Cliente | World of Warcraft **3.3.5a** (build 12340) |
| C++ | C++20 |
| CMake | ≥ 3.16 (Ubuntu 24.04 trae 3.27+) |
| Boost | ≥ 1.74 (Ubuntu 24.04 trae 1.83) |
| OpenSSL | ≥ 3.0 |
| Compilador | **clang** (recomendado; más rápido que gcc) |
| Base de datos | **MySQL 8.4 LTS** (Oracle). **No MariaDB.** |
| Usuario MySQL | `acore` / `acore` |
| Bases | `acore_auth`, `acore_world`, `acore_characters` |
| Authserver | puerto **3724** |
| Worldserver | puerto **8085** |
| Prefijo de install | `$HOME/azerothcore/env/dist` |

**No uses MariaDB.** Desde septiembre 2024 AzerothCore no lo soporta; el core
ni siquiera compila contra él. Tampoco uses MySQL 5.7 ni 8.1.

Ubuntu 24.04 **no** trae MySQL 8.4 en los repos oficiales (solo 8.0). Hay que
añadir el APT de Oracle y fijar la rama **mysql-8.4-lts**.

Hardware orientativo:

- RAM: **8 GB mínimo** para compilar; 16 GB cómodo. El enlace de `worldserver`
  puede quedarse sin memoria con 4 GB.
- Disco: ~15 GB para código + build, ~2 GB extra para `dbc/maps/vmaps/mmaps`.
- CPU: cuantos más núcleos, mejor. La primera compilación tarda bastante.

No compiles ni ejecutes el core como `root`. Usa un usuario normal con `sudo`.

---

## 1. Actualizar el sistema y paquetes base

```bash
sudo apt-get update
sudo apt-get upgrade -y
sudo apt-get install -y \
  ca-certificates curl wget gnupg lsb-release software-properties-common \
  git unzip zip screen tmux htop
```

Comprueba la versión:

```bash
lsb_release -a
# Distributor ID: Ubuntu
# Release:        24.04
```

### Swap (solo si tienes poca RAM)

Si `free -h` muestra menos de ~8 GB, crea 8 GB de swap antes de compilar:

```bash
sudo fallocate -l 8G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
echo '/swapfile none swap sw 0 0' | sudo tee -a /etc/fstab
```

### Desactivar actualizaciones automáticas de MySQL

Unattended-upgrades puede actualizar MySQL con el servidor en marcha y
tirar `authserver`/`worldserver`. Edita:

```bash
sudo nano /etc/apt/apt.conf.d/20auto-upgrades
```

Comenta todas las líneas (pon `//` al inicio) y reinicia cuando puedas.

---

## 2. Herramientas de compilación

```bash
sudo apt-get update
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
  git cmake make gcc g++ clang ccache \
  libssl-dev libbz2-dev libreadline-dev libncurses-dev \
  libboost-all-dev \
  gdb gdbserver expect jq
```

`libmysqlclient-dev` se instala **después** de añadir el repo de MySQL 8.4,
para no enlazar contra el cliente 8.0 de Ubuntu.

Verifica versiones:

```bash
clang --version      # Ubuntu 24.04: Clang 18.x
cmake --version      # debe ser ≥ 3.16
openssl version      # debe ser ≥ 3.0
g++ --version        # Ubuntu 24.04: GCC 13.x (C++20 OK)
```

---

## 3. Instalar MySQL 8.4 LTS (Oracle APT)

### 3.1 Clave GPG y paquete de configuración

La versión del metapaquete cambia. Comprueba la última en
<https://dev.mysql.com/downloads/repo/apt/>. A fecha de esta guía es
`0.8.40-1`. El instalador de este repo usa `0.8.35-1`; ambas sirven.

```bash
export MYSQL_APT_CONFIG_VERSION=0.8.40-1

# Clave del repo (evita el error de clave caducada)
sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys A8D3785C

wget "https://dev.mysql.com/get/mysql-apt-config_${MYSQL_APT_CONFIG_VERSION}_all.deb"
```

### 3.2 Forzar MySQL 8.4 LTS (sin menús interactivos)

Sin esto, el paquete puede elegir MySQL 8.0, 9.x u otra rama.

```bash
sudo debconf-set-selections <<EOF
mysql-apt-config mysql-apt-config/select-server select mysql-8.4-lts
mysql-apt-config mysql-apt-config/select-product select Ok
EOF

sudo DEBIAN_FRONTEND=noninteractive dpkg -i "./mysql-apt-config_${MYSQL_APT_CONFIG_VERSION}_all.deb"
rm -v "./mysql-apt-config_${MYSQL_APT_CONFIG_VERSION}_all.deb"
unset MYSQL_APT_CONFIG_VERSION
```

Si `dpkg` abre un menú de texto igual: entra en **MySQL Server & Cluster**,
elige **mysql-8.4-lts**, luego **Ok**.

### 3.3 Servidor + librería de desarrollo

```bash
sudo apt-get update
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y mysql-server libmysqlclient-dev
```

### 3.4 Arrancar y comprobar

```bash
sudo systemctl enable --now mysql.service
sudo systemctl status mysql.service --no-pager
mysql --version
# Debe mostrar 8.4.x, no 8.0.x ni MariaDB
```

Si ves `8.0.x` de Ubuntu:

```bash
apt-cache policy mysql-server
```

El candidato debe venir de `repo.mysql.com`, no de `ubuntu.com`. Si no,
repite 3.1–3.3 y no instales `mysql-server` de los repos de Ubuntu.

### 3.5 Acceso root

En Ubuntu, `root` de MySQL suele autenticarse por socket. Prueba:

```bash
sudo mysql -e "SELECT VERSION();"
```

Si pide contraseña (instalación interactiva previa):

```bash
sudo mysql -u root -p -e "SELECT VERSION();"
```

Opcional, endurecer (cambia el plugin de root; luego usa `-p`):

```bash
sudo mysql_secure_installation
```

Para un servidor privado en local **no es obligatorio**. El core **nunca**
debe conectarse como `root`; usa el usuario `acore`.

---

## 4. Crear usuario y bases de datos

El script del repo crea:

- usuario `'acore'@'localhost'` con password `acore`
- `acore_world`, `acore_characters`, `acore_auth` (utf8mb4)

Clona primero (paso 5) o descarga solo el SQL. Con el repo ya en disco:

```bash
# Sin password de root (auth_socket):
sudo mysql < "$HOME/azerothcore/data/sql/create/create_mysql.sql"

# Con password de root:
# sudo mysql -u root -p < "$HOME/azerothcore/data/sql/create/create_mysql.sql"
```

Si aún no has clonado, pega esto en `sudo mysql`:

```sql
DROP USER IF EXISTS 'acore'@'localhost';
CREATE USER 'acore'@'localhost' IDENTIFIED BY 'acore'
  WITH MAX_QUERIES_PER_HOUR 0 MAX_CONNECTIONS_PER_HOUR 0 MAX_UPDATES_PER_HOUR 0;

CREATE DATABASE IF NOT EXISTS `acore_world` DEFAULT CHARACTER SET UTF8MB4 COLLATE utf8mb4_unicode_ci;
CREATE DATABASE IF NOT EXISTS `acore_characters` DEFAULT CHARACTER SET UTF8MB4 COLLATE utf8mb4_unicode_ci;
CREATE DATABASE IF NOT EXISTS `acore_auth` DEFAULT CHARACTER SET UTF8MB4 COLLATE utf8mb4_unicode_ci;

GRANT ALL PRIVILEGES ON `acore_world`.* TO 'acore'@'localhost' WITH GRANT OPTION;
GRANT ALL PRIVILEGES ON `acore_characters`.* TO 'acore'@'localhost' WITH GRANT OPTION;
GRANT ALL PRIVILEGES ON `acore_auth`.* TO 'acore'@'localhost' WITH GRANT OPTION;
FLUSH PRIVILEGES;
```

Cambia `'acore'` (password) si quieres. Entonces tendrás que editar los
`.conf` del core (paso 8).

Comprueba:

```bash
mysql -u acore -pacore -e "SHOW DATABASES;"
```

Las tablas **no** se importan a mano. `authserver` y `worldserver` aplican
el schema y los updates al arrancar (`Updates.EnableDatabases`).

---

## 5. Clonar este repositorio

```bash
export AC_CODE_DIR="$HOME/azerothcore"
git clone --branch master --single-branch \
  https://github.com/FelixDevMaster/azerothcore-wotlk.git \
  "$AC_CODE_DIR"
cd "$AC_CODE_DIR"
```

Si ya estás dentro del clone, usa ese directorio como `AC_CODE_DIR`.

Módulos que este fork incluye en `modules/` (se compilan con `-DMODULES=static`):

- `mod-challenge-modes`
- `mod-rbg-aio`
- `mod-warden-ni`

Otros directorios bajo `modules/` están en `.gitignore` salvo que los
añadas explícitamente.

---

## 6. Compilar

El build **tiene que ser out-of-source**. Compilar dentro de la raíz del
repo está bloqueado por CMake.

### 6.1 Directorio de build

```bash
cd "$AC_CODE_DIR"
mkdir -p build
cd build
```

### 6.2 CMake

`clang` es más rápido que `gcc` en este proyecto.

```bash
cmake "$AC_CODE_DIR" \
  -DCMAKE_INSTALL_PREFIX="$AC_CODE_DIR/env/dist" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DWITH_WARNINGS=1 \
  -DSCRIPTS=static \
  -DMODULES=static \
  -DAPPS_BUILD=all \
  -DTOOLS_BUILD=all
```

| Flag | Significado |
| --- | --- |
| `CMAKE_INSTALL_PREFIX` | Dónde caen binarios y `.conf` (`bin/` y `etc/`) |
| `CMAKE_BUILD_TYPE=RelWithDebInfo` | Optimizado con símbolos de debug. `Release` más rápido; `Debug` más lento |
| `SCRIPTS=static` | Scripts del core enlazados estáticamente |
| `MODULES=static` | Módulos de `modules/` enlazados estáticamente |
| `TOOLS_BUILD=all` | Extractores de maps/vmaps/mmaps. Usa `none` si bajas data pre-extraída |
| `BUILD_TESTING=ON` | Tests (Google Test). Opcional |

Opciones extra útiles:

```text
-DBUILD_TESTING=ON
-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
```

### 6.3 Compilar e instalar

```bash
export BUILD_CORES=$(($(nproc) - 1))
# si nproc es 1, usa 1:
[ "$BUILD_CORES" -lt 1 ] && BUILD_CORES=1

cmake --build . --config RelWithDebInfo -j"$BUILD_CORES"
cmake --install . --config RelWithDebInfo
```

Equivalente clásico: `make -j"$BUILD_CORES" && make install`.

Binarios y configs:

```text
$AC_CODE_DIR/env/dist/bin/authserver
$AC_CODE_DIR/env/dist/bin/worldserver
$AC_CODE_DIR/env/dist/etc/authserver.conf.dist
$AC_CODE_DIR/env/dist/etc/worldserver.conf.dist
$AC_CODE_DIR/env/dist/etc/modules/*.conf.dist
```

### 6.4 Atajo con el dashboard (`acore.sh`)

Instala deps (incluye MySQL 8.4 en 24.04), compila y crea las DB:

```bash
cd "$AC_CODE_DIR"
./acore.sh install-deps
./acore.sh compiler all
./acore.sh setup-db
./acore.sh client-data
```

Primera instalación todo-en-uno:

```bash
./acore.sh init
```

El dashboard instala en `$AC_CODE_DIR/env/dist` y usa clang + `Release` por
defecto. Para cambiar flags, copia `conf/dist/config.sh` a `conf/config.sh`.

---

## 7. Datos del cliente (dbc / maps / vmaps / mmaps)

Sin estos ficheros `worldserver` no arranca bien.

| Carpeta | Obligatorio |
| --- | --- |
| `dbc` | Sí |
| `maps` | Sí |
| `vmaps` | Muy recomendable |
| `mmaps` | Muy recomendable |
| `Cameras` | Recomendable |

### Opción A — data pre-extraída (cliente enUS)

Solo vale para cliente **enUS**. Versión actual del downloader: **v20.0**.

```bash
cd "$AC_CODE_DIR/env/dist/bin"
curl -L -o data.zip \
  https://github.com/wowgaming/client-data/releases/download/v20.0/data.zip
unzip -o data.zip
rm data.zip
```

O:

```bash
cd "$AC_CODE_DIR"
./acore.sh client-data
```

Debe quedar así (`tree -L 1` desde `env/dist/bin`):

```text
.
├── authserver
├── Cameras
├── data-version
├── dbc
├── maps
├── mmaps
├── vmaps
└── worldserver
```

### Opción B — extraer de tu propio cliente 3.3.5a

Copia a la carpeta del WoW (donde está `Wow.exe`):

```text
map_extractor
mmaps_generator
vmap4_assembler
vmap4_extractor
apps/extractor/extractor.sh
```

```bash
mkdir -p mmaps vmaps
./extractor.sh
```

No interrumpas vmaps ni mmaps. Cuando termine, mueve `dbc`, `maps`, `vmaps`,
`mmaps` y `Cameras` a `$AC_CODE_DIR/env/dist/bin/`.

---

## 8. Configurar authserver y worldserver

```bash
CONF="$AC_CODE_DIR/env/dist/etc"
cp -n "$CONF/authserver.conf.dist" "$CONF/authserver.conf"
cp -n "$CONF/worldserver.conf.dist" "$CONF/worldserver.conf"

# configs de módulos de este fork
mkdir -p "$CONF/modules"
for f in "$CONF/modules/"*.conf.dist; do
  [ -e "$f" ] || break
  dest="${f%.dist}"
  [ -f "$dest" ] || cp -v "$f" "$dest"
done
```

Conexión MySQL (formato `host;puerto;usuario;password;base`):

```ini
# authserver.conf y worldserver.conf
LoginDatabaseInfo     = "127.0.0.1;3306;acore;acore;acore_auth"

# solo worldserver.conf
WorldDatabaseInfo     = "127.0.0.1;3306;acore;acore;acore_world"
CharacterDatabaseInfo = "127.0.0.1;3306;acore;acore;acore_characters"
```

`DataDir` por defecto es `"."` (el directorio de los binarios). Si pusiste
los maps en `env/dist/bin`, **no lo toques**. Si están en otra ruta:

```ini
DataDir = "/ruta/absoluta/a/los/datos"
```

Autoupdater (valores por defecto; déjalos así la primera vez):

```ini
# worldserver.conf  (1=auth + 2=characters + 4=world → 7 = todas)
Updates.EnableDatabases = 7
Updates.AutoSetup       = 1

# authserver.conf
Updates.EnableDatabases = 1
Updates.AutoSetup       = 1
```

`mod-warden-ni` necesita Warden activo (ya viene así):

```ini
Warden.Enabled = 1
```

---

## 9. Primer arranque

Abre **dos** terminales. Primero auth, luego world. El worldserver crea e
importa las DB (puede tardar varios minutos la primera vez).

```bash
cd "$AC_CODE_DIR/env/dist/bin"
./authserver
```

```bash
cd "$AC_CODE_DIR/env/dist/bin"
./worldserver
```

Si pregunta `Database "acore_auth" does not exist / Do you want to create it?`,
pulsa Enter (`yes`).

Cuando veas la consola `AC>` en worldserver:

```text
account create MIUSUARIO MIPASSWORD
account set gmlevel MIUSUARIO 3 -1
```

- Nivel `3` = GM. `-1` = todos los realms.
- Niveles típicos: 0 jugador, 1–3 staff, 4 consola (no lo asignes a cuentas de juego).

Con el dashboard:

```bash
cd "$AC_CODE_DIR"
./acore.sh run-authserver
./acore.sh run-worldserver
```

---

## 10. Cliente WoW 3.3.5a

AzerothCore **no** reparte el cliente. Necesitas un 3.3.5a limpio.

En `Data/realmlist.wtf` del cliente:

```text
set realmlist 127.0.0.1
```

Si usas `Launcher.exe`, pon la misma IP en `set patchlist`.

No uses `localhost`; usa `127.0.0.1`.

---

## 11. Red, firewall y realmlist

Puertos TCP:

| Puerto | Servicio |
| --- | --- |
| 3724 | authserver |
| 8085 | worldserver |
| 3306 | MySQL — **no lo abras a Internet** |

UFW (si está activo):

```bash
sudo ufw allow 3724/tcp
sudo ufw allow 8085/tcp
sudo ufw status
```

Jugar solo en esta máquina: deja `realmlist.address = 127.0.0.1`.

LAN u otras PCs:

```bash
mysql -u acore -pacore acore_auth -e \
  "UPDATE realmlist SET address = '192.168.x.x' WHERE id = 1;"
```

Internet (IP pública o DNS, y reenvío 3724 + 8085 en el router):

```bash
mysql -u acore -pacore acore_auth -e \
  "UPDATE realmlist SET address = 'TU.IP.PUBLICA' WHERE id = 1;"
```

El `realmlist.wtf` del cliente debe usar **la misma** IP/DNS.

---

## 12. systemd (opcional)

Sustituye `azerothuser` y la ruta si no coinciden.

```bash
export AC_CODE_DIR="$HOME/azerothcore"
export AC_UNIT_USER="$(whoami)"

sudo tee /etc/systemd/system/ac-authserver.service << EOF
[Unit]
Description=AzerothCore Authserver
After=network.target mysql.service
StartLimitIntervalSec=0

[Service]
Type=simple
Restart=always
RestartSec=1
User=$AC_UNIT_USER
WorkingDirectory=$AC_CODE_DIR
ExecStart=$AC_CODE_DIR/acore.sh run-authserver

[Install]
WantedBy=multi-user.target
EOF

sudo tee /etc/systemd/system/ac-worldserver.service << EOF
[Unit]
Description=AzerothCore Worldserver
After=network.target mysql.service ac-authserver.service
StartLimitIntervalSec=0

[Service]
Type=simple
Restart=always
RestartSec=1
User=$AC_UNIT_USER
WorkingDirectory=$AC_CODE_DIR
ExecStart=/bin/screen -S worldserver -D -m $AC_CODE_DIR/acore.sh run-worldserver

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable --now ac-authserver ac-worldserver
```

Consola del worldserver (cuenta GM, etc.):

```bash
screen -r worldserver
# Detach: Ctrl+A, luego D
```

---

## 13. Actualizar el core

```bash
cd "$AC_CODE_DIR"
git pull
cd build
cmake "$AC_CODE_DIR" \
  -DCMAKE_INSTALL_PREFIX="$AC_CODE_DIR/env/dist" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DWITH_WARNINGS=1 \
  -DSCRIPTS=static \
  -DMODULES=static \
  -DAPPS_BUILD=all \
  -DTOOLS_BUILD=all
cmake --build . --config RelWithDebInfo -j"$(($(nproc) - 1))"
cmake --install . --config RelWithDebInfo
```

O: `./acore.sh pull` y `./acore.sh compiler build`.

Si se añaden o quitan ficheros fuente, vuelve a pasar CMake (no solo `--build`).
Los SQL nuevos los aplica el worldserver al arrancar; no hace falta
`mysql < ...` a mano.

---

## 14. Módulos de este fork (tras el primer boot)

Los `.conf` de módulos viven en `env/dist/etc/modules/`.

- **Challenge Modes:** NPC Keeper of Challenges (entry `190012`). SQL de
  characters se crea solo.
- **RBG / arenas:** NPCs `190010` y `190011`. UI AIO opcional (Eluna +
  `AIO.lua` en `lua_scripts/`). Sin Eluna, los comandos `.rbg` / `.solo` siguen
  funcionando. Detalle: `modules/mod-rbg-aio/README.md`.
- **Warden NI:** detecta el loader ni-v3 / “che paladin”. Requiere
  `Warden.Enabled = 1`.

---

## 15. Problemas frecuentes

| Síntoma | Qué mirar |
| --- | --- |
| `cmake` no encuentra MySQL | Instala `libmysqlclient-dev` **del repo Oracle 8.4**, no el de Ubuntu |
| Enlaza o arranca contra MariaDB | `apt-get purge mariadb-*` y usa MySQL 8.4 |
| `mysql --version` dice 8.0 | El APT de Ubuntu ganó; revisa `apt-cache policy mysql-server` |
| `Access denied for user 'acore'` | Password distinto en SQL vs `.conf`; usuario solo `@localhost` |
| `Maps directory does not exist` | Data en `env/dist/bin` o `DataDir` mal puesto |
| Linker `killed` / OOM | Añade swap (paso 1) o baja `-j` a 2 |
| Cliente no ve el realm | `realmlist.wtf` y `acore_auth.realmlist.address` deben coincidir; no uses `localhost` |
| `cannot find -lstdc++` | En 24.04 no debería; en 26.04 instala `libstdc++-16-dev` |
| Worldserver pide crear DB | Enter = yes. Si falla, el usuario `acore` no tiene `GRANT` |

Logs: el directorio de trabajo de los binarios (`env/dist/bin`) o `LogsDir`
si lo cambiaste.

---

## 16. Resumen copy-paste (máquina limpia Ubuntu 24.04)

Ajusta `AC_CODE_DIR` y el clone si ya tienes el repo.

```bash
set -euo pipefail
export AC_CODE_DIR="$HOME/azerothcore"

sudo apt-get update
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
  ca-certificates curl wget gnupg lsb-release git unzip \
  cmake make gcc g++ clang ccache \
  libssl-dev libbz2-dev libreadline-dev libncurses-dev \
  libboost-all-dev gdb gdbserver expect jq screen tmux

# MySQL 8.4 LTS
export MYSQL_APT_CONFIG_VERSION=0.8.40-1
sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys A8D3785C
wget "https://dev.mysql.com/get/mysql-apt-config_${MYSQL_APT_CONFIG_VERSION}_all.deb"
sudo debconf-set-selections <<EOF
mysql-apt-config mysql-apt-config/select-server select mysql-8.4-lts
mysql-apt-config mysql-apt-config/select-product select Ok
EOF
sudo DEBIAN_FRONTEND=noninteractive dpkg -i "./mysql-apt-config_${MYSQL_APT_CONFIG_VERSION}_all.deb"
rm -f "./mysql-apt-config_${MYSQL_APT_CONFIG_VERSION}_all.deb"
sudo apt-get update
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y mysql-server libmysqlclient-dev
sudo systemctl enable --now mysql.service
mysql --version

# Código
git clone --branch master --single-branch \
  https://github.com/FelixDevMaster/azerothcore-wotlk.git "$AC_CODE_DIR"
sudo mysql < "$AC_CODE_DIR/data/sql/create/create_mysql.sql"

# Build
mkdir -p "$AC_CODE_DIR/build"
cd "$AC_CODE_DIR/build"
cmake "$AC_CODE_DIR" \
  -DCMAKE_INSTALL_PREFIX="$AC_CODE_DIR/env/dist" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DWITH_WARNINGS=1 \
  -DSCRIPTS=static \
  -DMODULES=static \
  -DAPPS_BUILD=all \
  -DTOOLS_BUILD=all
BUILD_CORES=$(($(nproc) - 1)); [ "$BUILD_CORES" -lt 1 ] && BUILD_CORES=1
cmake --build . --config RelWithDebInfo -j"$BUILD_CORES"
cmake --install . --config RelWithDebInfo

# Data enUS + configs
cd "$AC_CODE_DIR/env/dist/bin"
curl -L -o data.zip https://github.com/wowgaming/client-data/releases/download/v20.0/data.zip
unzip -o data.zip && rm data.zip
CONF="$AC_CODE_DIR/env/dist/etc"
cp -n "$CONF/authserver.conf.dist" "$CONF/authserver.conf"
cp -n "$CONF/worldserver.conf.dist" "$CONF/worldserver.conf"
mkdir -p "$CONF/modules"
for f in "$CONF/modules/"*.conf.dist; do
  [ -e "$f" ] || break
  dest="${f%.dist}"
  [ -f "$dest" ] || cp "$f" "$dest"
done

echo "Listo. Arranca:"
echo "  cd $AC_CODE_DIR/env/dist/bin && ./authserver"
echo "  cd $AC_CODE_DIR/env/dist/bin && ./worldserver"
```

Después, en la consola del worldserver:

```text
account create admin password
account set gmlevel admin 3 -1
```

Cliente: `set realmlist 127.0.0.1` en `Data/realmlist.wtf`.

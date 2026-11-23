import bpy
import bmesh
import math
import os
import re
import shutil
import struct
import random
from mathutils import Vector

CLIENT_WORLD = r"C:\Muemu\Client\Data\World25"
CLIENT_OBJ   = r"C:\Muemu\Client\Data\Object25"
SERVER_ATT   = r"C:\Muemu\MuServer\Data\Terrain\Terrain25.att"
BLEND_OUT    = r"C:\Muemu\Art\Battlefield_Map24.blend"

os.makedirs(CLIENT_WORLD, exist_ok=True)
os.makedirs(CLIENT_OBJ, exist_ok=True)
os.makedirs(os.path.dirname(SERVER_ATT), exist_ok=True)
os.makedirs(os.path.dirname(BLEND_OUT), exist_ok=True)

# ---------------------------------------------------------------------------
# MU Online 0.97k Encryption Helpers (MapFileEncrypt + BuxConvert)
# ---------------------------------------------------------------------------
MAP_XOR_KEY = [
    0xd1, 0x73, 0x52, 0xf6, 0xd2, 0x9a, 0xcb, 0x27,
    0x3e, 0xaf, 0x59, 0x31, 0x37, 0xb3, 0xe7, 0xa2
]
BUX_KEY = [0xfc, 0xcf, 0xab]

def map_file_encrypt(data: bytes) -> bytes:
    out = bytearray(len(data))
    w_map_key = 0x5E
    for i, b in enumerate(data):
        enc = ((b + w_map_key) & 0xFF) ^ MAP_XOR_KEY[i % 16]
        out[i] = enc
        w_map_key = (enc + 0x3D) & 0xFF
    return bytes(out)

def bux_convert(data: bytes) -> bytes:
    out = bytearray(data)
    for i in range(len(out)):
        out[i] ^= BUX_KEY[i % 3]
    return bytes(out)

# ---------------------------------------------------------------------------
# Procedural JPEG -> MU .OZJ writer (using Blender's built-in image saver!)
# ---------------------------------------------------------------------------
def save_ozj_from_pixels(path_ozj: str, width: int, height: int, rgba_floats):
    img = bpy.data.images.new("_tmp_tex", width=width, height=height, alpha=False)
    img.pixels = rgba_floats
    tmp_jpg = path_ozj + ".tmp.jpg"
    img.filepath_raw = tmp_jpg
    img.file_format = 'JPEG'
    bpy.context.scene.render.image_settings.quality = 88
    img.save()
    bpy.data.images.remove(img)
    with open(tmp_jpg, "rb") as f:
        jpg = f.read()
    os.remove(tmp_jpg)
    # MU .OZJ header is first 24 bytes of JPEG duplicated before the JPEG stream
    with open(path_ozj, "wb") as f:
        f.write(jpg[:24] + jpg)

def make_noise_tex(path_ozj: str, size: int, base_rgb, var_rgb, mode="noise"):
    rng = random.Random(hash(path_ozj) & 0xFFFFFFFF)
    pix = [0.0] * (size * size * 4)
    for y in range(size):
        for x in range(size):
            n = (rng.random() - 0.5) * 2.0
            if mode == "stone":
                brick = ((y // 16) % 2) * 16
                mortar = (y % 16 == 0) or ((x + brick) % 32 == 0)
                f = -0.35 if mortar else n * 0.6
            elif mode == "wood":
                grain = math.sin(y * 0.45 + math.sin(x * 0.08) * 2.5) * 0.5
                plank = (x % 32 == 0)
                f = -0.4 if plank else (grain * 0.5 + n * 0.35)
            elif mode == "leaf":
                clump = math.sin(x * 0.25) * math.cos(y * 0.25)
                f = clump * 0.6 + n * 0.5
            elif mode == "water":
                wave = math.sin(x * 0.18 + y * 0.12) * 0.5 + math.cos(y * 0.22) * 0.4
                f = wave * 0.6 + n * 0.25
            elif mode == "banner":
                stripe = 0.25 if (x < size * 0.15 or x > size * 0.85) else -0.1
                f = stripe + n * 0.15
            else:
                f = n * 0.65
            r = max(0.0, min(1.0, base_rgb[0] + var_rgb[0] * f))
            g = max(0.0, min(1.0, base_rgb[1] + var_rgb[1] * f))
            b = max(0.0, min(1.0, base_rgb[2] + var_rgb[2] * f))
            idx = (y * size + x) * 4
            pix[idx] = r; pix[idx + 1] = g; pix[idx + 2] = b; pix[idx + 3] = 1.0
    save_ozj_from_pixels(path_ozj, size, size, pix)

# ---------------------------------------------------------------------------
# Export Blender Mesh Object(s) -> Native MU 0.97k BMD (ver = 0x0A)
# ---------------------------------------------------------------------------
def export_bmd_v0a(filepath: str, mesh_entries, num_actions: int = 1):
    """
    mesh_entries: list of (bpy_obj, texture_filename_e_g_bf_stone_jpg)
    Writes native unencrypted BMD ver 0x0A compatible with MU 0.97k.
    """
    buf = bytearray()
    buf += b"BMD\x0a"
    name_bytes = os.path.basename(filepath).encode("ascii")[:32]
    buf += name_bytes.ljust(32, b"\x00")
    buf += struct.pack("<hhh", len(mesh_entries), 1, num_actions)  # NumMeshs, NumBones=1, NumActions

    for mesh_idx, (obj, tex_name) in enumerate(mesh_entries):
        depsgraph = bpy.context.evaluated_depsgraph_get()
        eval_obj = obj.evaluated_get(depsgraph)
        me = eval_obj.to_mesh()
        bm = bmesh.new()
        bm.from_mesh(me)
        bmesh.ops.triangulate(bm, faces=bm.faces[:])
        bm.to_mesh(me)
        bm.free()

        me.calc_loop_triangles()
        uv_layer = me.uv_layers.active.data if me.uv_layers.active else None

        # Vertices (Node=0, Position[3]) -> 16 bytes each (short Node, short pad, 3 floats)
        verts = []
        for v in me.vertices:
            co = obj.matrix_world @ v.co
            # MU coordinates: 100 units = 1 tile
            verts.append((co.x * 100.0, co.y * 100.0, co.z * 100.0))

        # Normals (Node=0, Normal[3], BindVertex=i) -> 20 bytes each
        norms = []
        for i, v in enumerate(me.vertices):
            no = (obj.matrix_world.to_3x3() @ v.normal).normalized()
            norms.append((no.x, no.y, no.z, i))

        # TexCoords (deduplicated so NumTexCoords stays tiny & 100% safe for MU 0.97k!)
        texcoords = []
        tc_map = {}
        tris = []
        for lt in me.loop_triangles:
            v_ids = [lt.vertices[0], lt.vertices[1], lt.vertices[2]]
            fn = lt.normal
            ax, ay, az = abs(fn.x), abs(fn.y), abs(fn.z)
            t_ids = []
            for li in lt.loops:
                if uv_layer:
                    uv = uv_layer[li].uv
                    u, v_c = round(float(uv.x), 3), round(float(1.0 - uv.y), 3)
                else:
                    co = verts[me.loops[li].vertex_index]
                    if az >= ax and az >= ay:
                        u, v_c = round(co[0] * 0.005, 3), round(co[1] * 0.005, 3)
                    elif ax >= ay:
                        u, v_c = round(co[1] * 0.005, 3), round(co[2] * 0.005, 3)
                    else:
                        u, v_c = round(co[0] * 0.005, 3), round(co[2] * 0.005, 3)
                key = (u, v_c)
                tid = tc_map.get(key)
                if tid is None:
                    tid = len(texcoords)
                    tc_map[key] = tid
                    texcoords.append(key)
                t_ids.append(tid)
            tris.append((v_ids, v_ids, t_ids))

        buf += struct.pack("<hhhhh", len(verts), len(norms), len(texcoords), len(tris), mesh_idx)
        for vx, vy, vz in verts:
            buf += struct.pack("<hhfff", 0, 0, vx, vy, vz)
        for nx, ny, nz, bv in norms:
            buf += struct.pack("<hhfffh2x", 0, 0, nx, ny, nz, bv)
        for u, v_c in texcoords:
            buf += struct.pack("<ff", u, v_c)
        for v_ids, n_ids, t_ids in tris:
            # Triangle_t (EXACT 64 bytes: 1+1+8+8+8+8+1+29 = 64)
            buf += struct.pack(
                "<BB4h4h4h4hB29x",
                3, 0,
                v_ids[0], v_ids[1], v_ids[2], 0,
                n_ids[0], n_ids[1], n_ids[2], 0,
                t_ids[0], t_ids[1], t_ids[2], 0,
                0, 0, 0, 0,
                0
            )
        tex_b = tex_name.encode("ascii")[:32]
        buf += tex_b.ljust(32, b"\x00")
        eval_obj.to_mesh_clear()

    # Action_t (num_actions actions, 1 keyframe each, LockPositions=0)
    for _ in range(num_actions):
        buf += struct.pack("<hB", 1, 0)
    # Bone_t (1 bone: Dummy=0, Name="Root", Parent=-1, 1 keyframe per action Pos=(0,0,0), Rot=(0,0,0))
    buf += struct.pack("<B", 0)
    buf += b"Root".ljust(32, b"\x00")
    buf += struct.pack("<h", -1)
    for _ in range(num_actions):
        buf += struct.pack("<3f3f", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)

    with open(filepath, "wb") as f:
        f.write(buf)

# ---------------------------------------------------------------------------
# Clean Blender Scene & Build 6 Authentic 3D Battlefield Models
# ---------------------------------------------------------------------------
bpy.ops.wm.read_factory_settings(use_empty=True)

def new_mesh_obj(name, bm):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(obj)
    return obj

def add_box(bm, cx, cy, cz, sx, sy, sz):
    res = bmesh.ops.create_cube(bm, size=1.0)
    for v in res["verts"]:
        v.co.x = v.co.x * sx + cx
        v.co.y = v.co.y * sy + cy
        v.co.z = v.co.z * sz + cz

def add_cyl(bm, cx, cy, cz, r1, r2, depth, segs=10):
    res = bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=segs, radius1=r1, radius2=r2, depth=depth)
    for v in res["verts"]:
        v.co.x += cx; v.co.y += cy; v.co.z += cz

# --- 1. WATCHTOWER (Thap Canh) -> Object01.bmd ---
bm_wt_stone = bmesh.new()
add_cyl(bm_wt_stone, 0, 0, 0.35, 1.45, 1.30, 0.70, 8)   # Stone pedestal
add_cyl(bm_wt_stone, 0, 0, 2.00, 1.15, 0.92, 2.70, 8)   # Tapered stone tower body
add_cyl(bm_wt_stone, 0, 0, 3.55, 1.35, 1.35, 0.45, 8)   # Upper stone watch parapet
for a in range(8):
    ang = a * math.pi / 4.0
    add_box(bm_wt_stone, math.cos(ang) * 1.18, math.sin(ang) * 1.18, 3.92, 0.42, 0.28, 0.36)
obj_wt_stone = new_mesh_obj("Watchtower_Stone", bm_wt_stone)

bm_wt_wood = bmesh.new()
for a in range(4):
    ang = a * math.pi / 2.0 + math.pi / 4.0
    add_box(bm_wt_wood, math.cos(ang) * 0.95, math.sin(ang) * 0.95, 4.55, 0.16, 0.16, 1.30)
add_cyl(bm_wt_wood, 0, 0, 5.45, 1.55, 0.12, 1.10, 8)    # Pointed roof
obj_wt_wood = new_mesh_obj("Watchtower_Roof", bm_wt_wood)

bm_wt_orb = bmesh.new()
add_cyl(bm_wt_orb, 0, 0, 6.25, 0.32, 0.05, 0.75, 6)     # Energy crystal spire
obj_wt_orb = new_mesh_obj("Watchtower_Crystal", bm_wt_orb)

export_bmd_v0a(os.path.join(CLIENT_OBJ, "Object01.bmd"), [
    (obj_wt_stone, "bf_stone.jpg"),
    (obj_wt_wood, "bf_wood.jpg"),
    (obj_wt_orb, "bf_crystal.jpg"),
])

# --- 2. SUSPENSION BRIDGE (Cau Treo - clean MU 0.97k low-poly < 200 tris!) -> Object02.bmd ---
bm_br_wood = bmesh.new()
span = 11.0
# Single clean solid planked bridge deck flush at z = -0.08 + 2 side curbs
add_box(bm_br_wood, 0.0, 0.0, -0.08, span, 4.6, 0.16)
add_box(bm_br_wood, 0.0, -2.25, 0.08, span, 0.22, 0.22)
add_box(bm_br_wood, 0.0,  2.25, 0.08, span, 0.22, 0.22)
# 4 heavy end bridge towers + 2 overhead crossbeams
for sx in (-span * 0.48, span * 0.48):
    for sy in (-2.35, 2.35):
        add_box(bm_br_wood, sx, sy, 1.45, 0.36, 0.36, 3.10)
    add_box(bm_br_wood, sx, 0.0, 2.85, 0.26, 5.0, 0.26)
# 6 side railing posts (3 per side)
for sy in (-2.25, 2.25):
    for x in (-2.8, 0.0, 2.8):
        add_box(bm_br_wood, x, sy, 0.52, 0.14, 0.14, 1.05)
obj_br_wood = new_mesh_obj("SuspensionBridge_Wood", bm_br_wood)

bm_br_rope = bmesh.new()
# Clean 6-segment catenary suspension cables + vertical hangers on both sides
for sy in (-2.30, 2.30):
    segs = 6
    for i in range(segs):
        tm = ((i + 0.5) / segs) - 0.5
        x = tm * span
        z = 1.05 + 1.75 * ((2.0 * tm) ** 2)
        add_box(bm_br_rope, x, sy, z, span / segs + 0.10, 0.12, 0.12)
        if i % 2 == 0:
            add_box(bm_br_rope, x, sy, z * 0.5, 0.08, 0.08, z)
obj_br_rope = new_mesh_obj("SuspensionBridge_Cables", bm_br_rope)

export_bmd_v0a(os.path.join(CLIENT_OBJ, "Object02.bmd"), [
    (obj_br_wood, "bf_wood.jpg"),
    (obj_br_rope, "bf_rope.jpg"),
])

# --- 3. LUSH GREEN TREE (Cay Xanh) -> Object03.bmd ---
bm_tr_bark = bmesh.new()
add_cyl(bm_tr_bark, 0, 0, 1.25, 0.36, 0.20, 2.50, 8)
obj_tr_bark = new_mesh_obj("Tree_Trunk", bm_tr_bark)

bm_tr_leaf = bmesh.new()
add_cyl(bm_tr_leaf, 0, 0, 2.40, 1.85, 0.35, 1.80, 9)
add_cyl(bm_tr_leaf, 0, 0, 3.55, 1.45, 0.25, 1.65, 9)
add_cyl(bm_tr_leaf, 0, 0, 4.55, 0.95, 0.06, 1.40, 8)
obj_tr_leaf = new_mesh_obj("Tree_Canopy", bm_tr_leaf)

export_bmd_v0a(os.path.join(CLIENT_OBJ, "Object03.bmd"), [
    (obj_tr_bark, "bf_wood.jpg"),
    (obj_tr_leaf, "bf_leaf.jpg"),
])

# --- 4. FORTRESS STONE WALL (Tuong Thanh) -> Object04.bmd ---
bm_wall = bmesh.new()
add_box(bm_wall, 0, 0, 1.15, 3.6, 0.85, 2.30)
for mx in (-1.35, -0.45, 0.45, 1.35):
    add_box(bm_wall, mx, 0, 2.55, 0.55, 0.90, 0.50)
obj_wall = new_mesh_obj("Fortress_Wall", bm_wall)
export_bmd_v0a(os.path.join(CLIENT_OBJ, "Object04.bmd"), [(obj_wall, "bf_stone.jpg")])

# --- 5. WAR BANNER (Co Chien Truong) -> Object05.bmd ---
bm_ban_pole = bmesh.new()
add_cyl(bm_ban_pole, 0, 0, 1.90, 0.09, 0.06, 3.80, 6)
add_box(bm_ban_pole, 0, 0, 3.55, 1.40, 0.08, 0.08)
obj_ban_pole = new_mesh_obj("Banner_Pole", bm_ban_pole)

bm_ban_cloth = bmesh.new()
add_box(bm_ban_cloth, 0, 0.06, 2.55, 1.15, 0.05, 1.85)
obj_ban_cloth = new_mesh_obj("Banner_Cloth", bm_ban_cloth)

export_bmd_v0a(os.path.join(CLIENT_OBJ, "Object05.bmd"), [
    (obj_ban_pole, "bf_wood.jpg"),
    (obj_ban_cloth, "bf_banner.jpg"),
])

# --- 6. MOSSY BOULDER (Da Reu) -> Object06.bmd ---
bm_rock = bmesh.new()
bmesh.ops.create_icosphere(bm_rock, subdivisions=2, radius=1.0)
rng = random.Random(42)
for v in bm_rock.verts:
    v.co.x *= 1.35 + rng.uniform(-0.18, 0.18)
    v.co.y *= 1.15 + rng.uniform(-0.18, 0.18)
    v.co.z = max(0.0, (v.co.z + 0.55) * 0.72 + rng.uniform(-0.10, 0.10))
obj_rock = new_mesh_obj("Mossy_Boulder", bm_rock)
export_bmd_v0a(os.path.join(CLIENT_OBJ, "Object06.bmd"), [(obj_rock, "bf_rock.jpg")])

# --- 7. ARCANE CRYSTAL FLAG TOWER (Tru Co Chien Truong - replaces ugly Statue 132!) -> FlagTower.bmd ---
bm_ft_stone = bmesh.new()
add_cyl(bm_ft_stone, 0, 0, 0.20, 1.18, 1.02, 0.40, 8)   # Wide octagonal stepped altar base
add_cyl(bm_ft_stone, 0, 0, 0.88, 0.86, 0.68, 0.96, 8)   # Tapered runic stone monolith plinth
add_cyl(bm_ft_stone, 0, 0, 1.52, 0.64, 0.92, 0.34, 8)   # Flared Gothic chalice crown
for a in range(4):
    ang = a * math.pi / 2.0 + math.pi / 4.0
    cx, cy = math.cos(ang), math.sin(ang)
    add_box(bm_ft_stone, cx * 0.78, cy * 0.78, 2.05, 0.24, 0.24, 0.90)  # Lower Gothic claw
    add_box(bm_ft_stone, cx * 0.64, cy * 0.64, 2.72, 0.18, 0.18, 0.56)  # Upper inward claw tip
obj_ft_stone = new_mesh_obj("FlagTower_Stone", bm_ft_stone)

bm_ft_steel = bmesh.new()
add_cyl(bm_ft_steel, 0, 0, 0.44, 1.06, 1.06, 0.12, 8)   # Forged dark-steel base band
add_cyl(bm_ft_steel, 0, 0, 1.69, 0.96, 0.96, 0.12, 8)   # Forged chalice rim collar
for a in range(4):
    ang = a * math.pi / 2.0
    cx, cy = math.cos(ang), math.sin(ang)
    add_cyl(bm_ft_steel, cx * 0.96, cy * 0.96, 1.15, 0.08, 0.02, 1.55, 6) # 4 forged battle-spears
    add_box(bm_ft_steel, cx * 0.72, cy * 0.72, 3.10, 0.14, 0.56, 0.10)    # Crown halo bars
obj_ft_steel = new_mesh_obj("FlagTower_Steel", bm_ft_steel)

bm_ft_gem = bmesh.new()
# Giant floating faceted diamond energy crystal core
add_cyl(bm_ft_gem, 0, 0, 2.38, 0.04, 0.48, 0.85, 6)     # Lower crystal pyramid
add_cyl(bm_ft_gem, 0, 0, 3.45, 0.48, 0.03, 1.30, 6)     # Tall glowing crystal spire
# 2 smaller floating energy shards flanking the main crystal
for sx in (-0.70, 0.70):
    add_cyl(bm_ft_gem, sx, 0, 2.55, 0.02, 0.16, 0.36, 4)
    add_cyl(bm_ft_gem, sx, 0, 2.95, 0.16, 0.02, 0.46, 4)
obj_ft_gem = new_mesh_obj("FlagTower_Crystal", bm_ft_gem)

export_bmd_v0a(os.path.join(CLIENT_OBJ, "FlagTower.bmd"), [
    (obj_ft_stone, "bf_stone.jpg"),
    (obj_ft_steel, "bf_steel.jpg"),
    (obj_ft_gem,   "bf_crystal.jpg"),
], num_actions=7)

# Save Blender project file so user can inspect/edit in Blender
bpy.ops.wm.save_as_mainfile(filepath=BLEND_OUT)

# ---------------------------------------------------------------------------
# Use REAL hand-painted MU Online Textures from World1 & Object1 for AAA realism!
# ---------------------------------------------------------------------------
W1 = r"C:\Muemu\Client\Data\World1"
O1 = r"C:\Muemu\Client\Data\Object1"

def copy_tex(src, dst):
    if os.path.exists(src):
        shutil.copy2(src, dst)

# Real MU textures for our Blender 3D Watchtowers, Suspension Bridges, Walls, Boulders
copy_tex(os.path.join(O1, "c_wall04.OZJ"),           os.path.join(CLIENT_OBJ, "bf_stone.OZJ"))
copy_tex(os.path.join(W1, "TileWood01.OZJ"),         os.path.join(CLIENT_OBJ, "bf_wood.OZJ"))
copy_tex(os.path.join(W1, "TileWood01.OZJ"),         os.path.join(CLIENT_OBJ, "bf_rope.OZJ"))
copy_tex(os.path.join(O1, "fire_01.OZJ"),            os.path.join(CLIENT_OBJ, "bf_crystal.OZJ"))
copy_tex(os.path.join(O1, "steel.OZJ"),              os.path.join(CLIENT_OBJ, "bf_steel.OZJ"))
copy_tex(os.path.join(O1, "angel_stone_statue.OZJ"), os.path.join(CLIENT_OBJ, "bf_rock.OZJ"))
make_noise_tex(os.path.join(CLIENT_OBJ, "bf_banner.OZJ"), 128, (0.68, 0.14, 0.14), (0.14, 0.08, 0.06), "banner")

# Helper: copy any native MU 0.97k BMD + ALL its referenced textures into Object25
def import_mu_bmd(src_dir, src_bmd, dst_bmd):
    src_path = os.path.join(src_dir, src_bmd)
    if not os.path.exists(src_path):
        return False
    shutil.copy2(src_path, os.path.join(CLIENT_OBJ, dst_bmd))
    td = open(src_path, "rb").read()
    for m in re.findall(rb"[A-Za-z0-9_\-]+\.(?:jpg|tga|JPG|TGA)", td):
        base = os.path.splitext(m.decode("ascii", "ignore"))[0]
        for ext in (".OZJ", ".OZT", ".ozj", ".ozt"):
            sp = os.path.join(src_dir, base + ext)
            if os.path.exists(sp):
                shutil.copy2(sp, os.path.join(CLIENT_OBJ, base + ext))
    return True

O12 = r"C:\Muemu\Client\Data\Object12"
# Authentic MU 3D Foliage & War Camp Props -> Object03 + Object07..Object12
import_mu_bmd(O1,  "Tree01.bmd",      "Object03.bmd") # Lush MU Tree
import_mu_bmd(O1,  "Bonfire01.bmd",   "Object07.bmd") # Camp Bonfire
import_mu_bmd(O1,  "Cannon01.bmd",    "Object08.bmd") # Siege Cannon
import_mu_bmd(O1,  "Carriage01.bmd",  "Object09.bmd") # Supply Carriage
import_mu_bmd(O1,  "FireLight01.bmd", "Object10.bmd") # Flaming Torch Brazier
import_mu_bmd(O12, "Gate01.bmd",      "Object11.bmd") # Grand Gothic Base Gate
import_mu_bmd(O12, "Object05.bmd",    "Object12.bmd") # Gothic Stone Pillar

# Copy ALL authentic Lorencia World1 terrain tiles (grass, cobblestone, rock, water, wood, grass tufts)
for fn in os.listdir(W1):
    low = fn.lower()
    if low.startswith(("tile", "exttile", "leaf", "rain")):
        shutil.copy2(os.path.join(W1, fn), os.path.join(CLIENT_WORLD, fn))
for extra in ("TileRock03.OZJ", "TileRock04.OZJ", "TileRock05.OZJ", "TileRock06.OZJ", "TileRock07.OZJ"):
    if not os.path.exists(os.path.join(CLIENT_WORLD, extra)):
        copy_tex(os.path.join(W1, "TileRock01.OZJ"), os.path.join(CLIENT_WORLD, extra))
for extra in ("ExtTile01.OZJ", "ExtTile02.OZJ"):
    if not os.path.exists(os.path.join(CLIENT_WORLD, extra)):
        copy_tex(os.path.join(W1, "TileGround01.OZJ"), os.path.join(CLIENT_WORLD, extra))
for gname in ("TileGrass02.OZT", "TileGrass03.OZT"):
    if not os.path.exists(os.path.join(CLIENT_WORLD, gname)):
        copy_tex(os.path.join(W1, "TileGrass01.OZT"), os.path.join(CLIENT_WORLD, gname))

# ---------------------------------------------------------------------------
# Build 256x256 Terrain Attributes, Height, Map Tiles, Lighting
# ---------------------------------------------------------------------------
W = 256
att = bytearray(W * W)
layer1 = bytearray(W * W)
layer2 = bytearray(W * W)
alpha  = bytearray(W * W)
height = bytearray(W * W)
light_pix = [0.0] * (W * W * 4)

def in_bridge(x, y):
    # Narrow dramatic bridge deck crossing the abyss at Top (74,182), Mid (128,128), Bot (182,74)
    # Along diagonal (x - y = const): bridge width = 5 tiles, span across chasm = 15 tiles
    if abs((x - y) - (74 - 182)) <= 3 and abs((x + y) - 256) <= 8: return True
    if abs((x - y) - 0)          <= 4 and abs((x + y) - 256) <= 8: return True
    if abs((x - y) - (182 - 74)) <= 3 and abs((x + y) - 256) <= 8: return True
    return False

def on_road(x, y):
    # Blue Base & Red Base plazas
    if 36 <= x <= 62 and 36 <= y <= 62: return True
    if 194 <= x <= 220 and 194 <= y <= 220: return True
    # Top Lane: West road up to Top Bridge then East to Red Base
    if 46 <= x <= 62 and 56 <= y <= 188: return True
    if 56 <= x <= 200 and 176 <= y <= 192: return True
    if 194 <= x <= 210 and 176 <= y <= 202: return True
    # Bot Lane: South road East to Bot Bridge then North to Red Base
    if 56 <= x <= 192 and 46 <= y <= 62: return True
    if 176 <= x <= 192 and 56 <= y <= 200: return True
    # Mid Lane: Diagonal highway connecting Blue Base -> Mid Bridge -> Red Base
    if 52 <= x <= 204 and 52 <= y <= 204 and abs(x - y) <= 9: return True
    return False

# Compute smooth organic road blending (Layer1=Grass, Layer2=Cobblestone, Alpha=smooth gradient!)
road_mask = [[1.0 if on_road(x, y) else 0.0 for x in range(W)] for y in range(W)]

# Torch/Landmark light sources for atmospheric MU Online lighting
lights = [
    (48, 48, 28.0, 0.35, 0.45, 0.70),   # Blue Base cool-blue glow
    (208, 208, 28.0, 0.70, 0.32, 0.25), # Red Base crimson-amber glow
    (74, 182, 20.0, 0.55, 0.48, 0.30),  # Top Bridge warm torchlight
    (128, 128, 24.0, 0.60, 0.52, 0.32), # Mid Bridge golden spotlight
    (182, 74, 20.0, 0.55, 0.48, 0.30),  # Bot Bridge warm torchlight
    (54, 130, 14.0, 0.25, 0.45, 0.65),  # Blue Watchtowers
    (92, 92, 14.0, 0.25, 0.45, 0.65),
    (130, 54, 14.0, 0.25, 0.45, 0.65),
    (130, 202, 14.0, 0.65, 0.28, 0.22), # Red Watchtowers
    (164, 164, 14.0, 0.65, 0.28, 0.22),
    (202, 130, 14.0, 0.65, 0.28, 0.22),
]

for y in range(W):
    for x in range(W):
        idx = y * W + x
        in_arena = (32 <= x <= 224 and 32 <= y <= 224)
        riv_d = abs((x + y) - 256)
        is_chasm = (riv_d <= 6)
        br = in_bridge(x, y)

        # Smooth 5x5 average of road_mask for organic feathered road borders!
        rsum = 0.0
        for dy in (-2, -1, 0, 1, 2):
            yy = max(0, min(W - 1, y + dy))
            row = road_mask[yy]
            for dx in (-2, -1, 0, 1, 2):
                rsum += row[max(0, min(W - 1, x + dx))]
        road_blend = rsum / 25.0

        # Subtle organic terrain hills in grass areas
        hill = int(math.sin(x * 0.09) * math.cos(y * 0.09) * 5.0 + math.sin((x + y) * 0.05) * 3.0)

        if not in_arena:
            att[idx] = 4
            layer1[idx] = 7
            layer2[idx] = 7
            alpha[idx] = 0
            height[idx] = 148 + hill
            lr, lg, lb = 0.28, 0.30, 0.34
        elif is_chasm and not br:
            # Deep bottomless abyss chasm (attr 4|8 = unwalkable + void no-ground, height = 0!)
            att[idx] = 12 if riv_d <= 4 else 4
            layer1[idx] = 7  # dark cliff rock on edges
            layer2[idx] = 7
            alpha[idx] = 0
            height[idx] = 0 if riv_d <= 4 else (35 + (riv_d - 5) * 35)
            lr, lg, lb = (0.02, 0.02, 0.04) if riv_d <= 4 else (0.18, 0.19, 0.22)
        elif br:
            att[idx] = 0
            layer1[idx] = 6  # TileWood01
            layer2[idx] = 2
            alpha[idx] = 0
            height[idx] = 104
            lr, lg, lb = 0.72, 0.66, 0.56
        else:
            if (38 <= x <= 58 and 38 <= y <= 58) or (198 <= x <= 218 and 198 <= y <= 218):
                att[idx] = 1
            else:
                att[idx] = 0
            # Layer1 = Lorencia Grass, Layer2 = Lorencia Cobblestone Road, Alpha = smooth feathered blend!
            layer1[idx] = 0 if ((x // 3 + y // 3) & 1) == 0 else 1
            layer2[idx] = 2  # TileGround01 (Lorencia stone)
            alpha[idx] = max(0, min(255, int(road_blend * 255.0)))
            height[idx] = max(60, min(180, 100 + int((1.0 - road_blend) * hill)))
            lr, lg, lb = 0.52, 0.56, 0.50

        # Add warm/cool landmark lighting pools
        for (lx, ly, rad, cr, cg, cb) in lights:
            d2 = (x - lx) * (x - lx) + (y - ly) * (y - ly)
            if d2 < rad * rad:
                f = (1.0 - math.sqrt(d2) / rad) ** 1.6
                lr += cr * f; lg += cg * f; lb += cb * f

        p = idx * 4
        light_pix[p]     = max(0.0, min(1.0, lr))
        light_pix[p + 1] = max(0.0, min(1.0, lg))
        light_pix[p + 2] = max(0.0, min(1.0, lb))
        light_pix[p + 3] = 1.0

# Save Client EncTerrain25.att (version=0, map=25, width=255, height=255 -> BuxConvert -> MapFileEncrypt)
att_raw_client = bytes([0, 25, 255, 255]) + bytes(att)
with open(os.path.join(CLIENT_WORLD, "EncTerrain25.att"), "wb") as f:
    f.write(map_file_encrypt(bux_convert(att_raw_client)))

# Save Server Terrain25.att (3-byte header [0, 255, 255] + 65536 bytes)
with open(SERVER_ATT, "wb") as f:
    f.write(bytes([0, 255, 255]) + bytes(att))

# Save EncTerrain25.map (2-byte header [0, 25] + Layer1[65536] + Layer2[65536] + Alpha[65536] -> MapFileEncrypt)
map_raw = bytes([0, 25]) + bytes(layer1) + bytes(layer2) + bytes(alpha)
with open(os.path.join(CLIENT_WORLD, "EncTerrain25.map"), "wb") as f:
    f.write(map_file_encrypt(map_raw))

# Save TerrainHeight.OZB (4-byte prefix + 1080-byte BMP header/palette + 65536 height bytes)
bmp_hdr = bytearray(1080)
bmp_hdr[0:2] = b"BM"
struct.pack_into("<I", bmp_hdr, 2, 1080 + W * W)
struct.pack_into("<I", bmp_hdr, 10, 1080)
struct.pack_into("<I", bmp_hdr, 14, 40)
struct.pack_into("<ii", bmp_hdr, 18, W, W)
struct.pack_into("<HH", bmp_hdr, 26, 1, 8)
for i in range(256):
    bmp_hdr[54 + i * 4 : 54 + i * 4 + 4] = bytes([i, i, i, 0])
with open(os.path.join(CLIENT_WORLD, "TerrainHeight.OZB"), "wb") as f:
    f.write(bytes(bmp_hdr[:4]) + bytes(bmp_hdr) + bytes(height))

# Save TerrainLight.OZJ
save_ozj_from_pixels(os.path.join(CLIENT_WORLD, "TerrainLight.OZJ"), W, W, light_pix)

# ---------------------------------------------------------------------------
# Build EncTerrain25.obj (Place all Blender 3D Objects across the 3-lane map!)
# ---------------------------------------------------------------------------
# Object types: 0=Watchtower, 1=SuspensionBridge, 2=Tree, 3=Wall, 4=Banner, 5=Boulder
objects = []

def place_obj(otype, gx, gy, gz_off=0.0, rot_z=0.0, scale=1.0):
    # MU world coords: X = gx * 100, Y = gy * 100, Z = height * 1.5
    h = height[int(gy) * W + int(gx)] * 1.5 + gz_off
    objects.append((otype, gx * 100.0, gy * 100.0, h, 0.0, 0.0, rot_z, scale))

# 1) 3 Suspension Bridges spanning straight across the deep abyssal chasm (45 deg)
place_obj(1, 74, 182, 0.0, 45.0, 1.65)
place_obj(1, 128, 128, 0.0, 45.0, 1.75)
place_obj(1, 182, 74, 0.0, 45.0, 1.65)

# 2) 6 Defensive Watchtowers (3 Blue + 3 Red)
for (tx, ty) in [(54, 130), (92, 92), (130, 54), (130, 202), (164, 164), (202, 130)]:
    place_obj(0, tx, ty, 0.0, 0.0, 1.15)

# 3) War Banners flanking the 3 Capture Flag Towers & Base Gates
for (bx, by) in [
    (69, 186), (79, 178), (123, 133), (133, 123), (177, 79), (187, 69),
    (62, 54), (54, 62), (194, 202), (202, 194)
]:
    place_obj(4, bx, by, 0.0, 45.0, 1.1)

# 4) Seamless Fortress Walls framing Blue Base & Red Base (no gaps!)
for wx in range(37, 60, 4):
    place_obj(3, wx, 36, 0.0, 0.0, 1.15)
    place_obj(3, 36, wx, 0.0, 90.0, 1.15)
for wx in range(197, 220, 4):
    place_obj(3, wx, 220, 0.0, 0.0, 1.15)
    place_obj(3, 220, wx, 0.0, 90.0, 1.15)

# 4B) Epic Medieval War Camp Waiting Areas (Blue Base 48,48 & Red Base 208,208)
# Types: 6=Bonfire, 7=Cannon, 8=Carriage, 9=Torch, 10=GothicGate, 11=GothicPillar
def decorate_base(cx, cy, is_blue):
    d = 1 if is_blue else -1
    # Central glowing War Bonfire + 4 Gothic corner pillars
    place_obj(6, cx + d * 4, cy + d * 4, 0.0, 0.0, 1.25)
    for (px, py) in [(cx - 7, cy - 7), (cx + 7, cy - 7), (cx - 7, cy + 7), (cx + 7, cy + 7)]:
        place_obj(11, px, py, 0.0, 0.0, 1.05)
    # 2 Siege Cannons aimed toward the battlefield lanes
    place_obj(7, cx + d * 10, cy + d * 2, 0.0, 45.0 if is_blue else 225.0, 1.15)
    place_obj(7, cx + d * 2, cy + d * 10, 0.0, 45.0 if is_blue else 225.0, 1.15)
    # Military Supply Carriage near the back wall
    place_obj(8, cx - d * 5, cy - d * 2, 0.0, 30.0 if is_blue else 210.0, 1.05)
    # Grand Gothic Gate facing Mid Lane + flanking flaming torches
    place_obj(10, cx + d * 12, cy + d * 12, 0.0, 45.0 if is_blue else 225.0, 1.1)
    for (tx, ty) in [
        (cx + d * 9, cy + d * 13), (cx + d * 13, cy + d * 9),
        (cx + d * 13, cy - d * 4), (cx - d * 4, cy + d * 13),
        (cx - d * 9, cy - d * 9), (cx + d * 1, cy - d * 9)
    ]:
        place_obj(9, tx, ty, 0.0, 0.0, 1.15)

decorate_base(48, 48, True)
decorate_base(208, 208, False)

# Flaming Torches flanking all 3 Suspension Bridges
for (tx, ty) in [(68, 176), (80, 188), (122, 122), (134, 134), (176, 68), (188, 80)]:
    place_obj(9, tx, ty, 0.0, 0.0, 1.15)

# 5) Lush Green Trees scattered in the jungle corridors between the 3 lanes
rng_t = random.Random(2026)
for ty in range(44, 214, 11):
    for tx in range(44, 214, 11):
        jx = tx + rng_t.randint(-3, 3)
        jy = ty + rng_t.randint(-3, 3)
        if not (38 <= jx <= 218 and 38 <= jy <= 218): continue
        if on_road(jx, jy) or in_bridge(jx, jy): continue
        if abs((jx + jy) - 256) <= 10: continue
        # Keep clear of watchtowers
        if any(abs(jx - wx) < 8 and abs(jy - wy) < 8 for (wx, wy) in [(54,130),(92,92),(130,54),(130,202),(164,164),(202,130)]):
            continue
        place_obj(2, jx, jy, 0.0, rng_t.uniform(0, 360), rng_t.uniform(0.85, 1.25))

# 6) Mossy Boulders along riverbanks
for k in range(45, 215, 14):
    rx, ry = k, 256 - k
    if in_bridge(rx, ry): continue
    place_obj(5, max(35, rx - 7), max(35, ry - 7), 0.0, rng_t.uniform(0, 360), rng_t.uniform(0.8, 1.3))
    place_obj(5, min(220, rx + 7), min(220, ry + 7), 0.0, rng_t.uniform(0, 360), rng_t.uniform(0.8, 1.3))

obj_raw = bytearray()
obj_raw += struct.pack("<BBH", 0, 25, len(objects))
for (otype, px, py, pz, ax, ay, az, sc) in objects:
    obj_raw += struct.pack("<h3f3ff", otype, px, py, pz, ax, ay, az, sc)

with open(os.path.join(CLIENT_WORLD, "EncTerrain25.obj"), "wb") as f:
    f.write(map_file_encrypt(bytes(obj_raw)))

print(f"BLENDER BATTLEFIELD MAP 24 GENERATED OK! Objects={len(objects)}, Blend={BLEND_OUT}")

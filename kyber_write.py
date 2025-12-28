import sys
import time
from tkinter import *
from tkinter import ttk, colorchooser, messagebox
from PIL import Image, ImageTk
from smartcard.System import readers
from smartcard.util import toBytes, toHexString
import ttkbootstrap as ttk
from ttkbootstrap.constants import *

#################### NFC FUNCTIONS ####################

def connect_reader():
    r = readers()
    if not r:
        raise Exception("No hay lectores NFC conectados.")
    reader = r[0]
    conn = reader.createConnection()
    conn.connect()
    
    # Desactivar el buzzer del ACR122U.
    try:
        buzz_off = [0xFF, 0x00, 0x52, 0x00, 0x00]
        conn.transmit(buzz_off)
    except:
        pass
    
    return conn

# Escribir página.
def ntag_write_page(conn, page, data4, retries=3):
    if len(data4) != 4:
        raise Exception("El bloque debe tener exactamente 4 bytes.")

    # APDU: FF D6 00 [page] 04 [data]
    cmd = [0xFF, 0xD6, 0x00, page, 0x04] + data4

    print(f"[WRITE] Página {page}: {data4} -> HEX: {toHexString(data4)}")
    print(f"[WRITE] Comando completo: {toHexString(cmd)}")
    
    last_error = None
    for attempt in range(retries):
        try:
            # Pausa progresivamente mayor en cada reintento.
            if attempt > 0:
                wait_time = 0.2 * (attempt + 1)  # 0.2s, 0.4s, 0.6s
                print(f"[WRITE] Reintento {attempt + 1}/{retries} (esperando {wait_time}s)")
                time.sleep(wait_time)
            
            resp, sw1, sw2 = conn.transmit(cmd)
            print(f"[WRITE] Respuesta SW: {sw1:02X} {sw2:02X}, Resp: {resp}")
            
            if (sw1, sw2) == (0x90, 0x00):
                # Éxito. Pausa más larga antes de siguiente operación.
                time.sleep(0.2)
                return
            elif (sw1, sw2) == (0x63, 0x00):
                # Error 63 00
                last_error = f"Error temporal al escribir página {page}: SW={sw1:02X}{sw2:02X}"
                print(f"[WRITE] {last_error}")
                continue
            else:
                raise Exception(f"Error al escribir página {page}: SW={sw1:02X}{sw2:02X}")
                
        except Exception as e:
            if "Error al escribir" in str(e):
                raise
            last_error = str(e)
            print(f"[WRITE] Excepción en intento {attempt + 1}: {e}")
    
    # Si todos los intentos fallaron...
    raise Exception(f"Falló después de {retries} intentos: {last_error}")

# Leer página.
def ntag_read_page(conn, page):
    # APDU: FF B0 00 [page] [length]
    cmd = [0xFF, 0xB0, 0x00, page, 0x04]
    
    resp, sw1, sw2 = conn.transmit(cmd)
    print(f"[READ] Página {page} - SW: {sw1:02X} {sw2:02X}")
    print(f"[READ] Respuesta completa: {resp}")
    
    if (sw1, sw2) != (0x90, 0x00):
        raise Exception(f"Error al leer página {page}: SW={sw1:02X}{sw2:02X}")

    data = resp[:4]
    print(f"[READ] Datos: {data} -> HEX: {toHexString(data)}")
    return data

 ################ GET UID ################

def ntag_get_uid(conn):
    cmd = [0xFF, 0xCA, 0x00, 0x00, 0x00]
    resp, sw1, sw2 = conn.transmit(cmd)

    if (sw1, sw2) != (0x90, 0x00):
        raise Exception(f"Error leyendo UID: SW={sw1:02X}{sw2:02X}")

    return resp

#################### GUI ####################

class App:
    def __init__(self, root):
        self.root = root
        self.root.title("Arkaivos Kyber Lab")

        self.color_rgb = (255, 255, 255)
        self.text_var = StringVar()
        self.owner_var = StringVar()
        self.color_hex_var = StringVar(value="#000000")
        self.firma_valida = IntVar(value=0)

        # Cargar imagen base.
        try:
            self.base_image = Image.open("kyber.png").convert("RGBA")
            # Redimensionar manteniendo aspect ratio 2:3.
            target_height = 120
            target_width = int(target_height * 2 / 3)
            self.base_image = self.base_image.resize((target_width, target_height), Image.LANCZOS)
        except Exception as e:
            messagebox.showerror("Error", f"No se pudo cargar la imagen: {e}")
            self.base_image = None

        self.build_ui()

    def build_ui(self):
        frm = ttk.Frame(self.root, padding=10)
        frm.grid(row=0, column=0)

        # Configurar pesos de columnas para distribuir mejor el espacio.
        frm.columnconfigure(1, weight=0)
        frm.columnconfigure(2, weight=1)

        # Color selector.
        ttk.Label(frm, text="Color RGB:").grid(row=0, column=0, sticky="w", pady=(0, 8))
        ttk.Entry(frm, textvariable=self.color_hex_var, width=10).grid(row=0, column=1, sticky="w", padx=5, pady=(0, 8))
        ttk.Button(frm, text="Select color", command=self.choose_color).grid(row=0, column=2, sticky="w", pady=(0, 8))

        self.style_label_color = "UIDLabel.TLabel"
        self.style = ttk.Style()
        self.style.configure(self.style_label_color, foreground="#FFFFFF")  # color inicial negro

        # Color preview
        if self.base_image:
            self.color_preview = ttk.Label(frm)
            self.color_preview.grid(row=0, column=3, padx=(30, 20), pady=(10, 0),rowspan= 3, sticky="n")
            self.update_color_preview()
        else:
            self.color_preview = Canvas(frm, width=40, height=20, bg="#000000")
            self.color_preview.grid(row=0, column=3, padx=10, sticky="e")

        # Text
        ttk.Label(frm, text="Preset: ").grid(row=1, column=0, sticky="w", pady=(0, 20))
        vcmd = (frm.register(self.validate_text), "%P")

        self.entry_text = ttk.Entry(
            frm,
            textvariable=self.text_var,
            width=25,
            validate="key",
            validatecommand=vcmd
        )
        self.entry_text.grid(row=1, column=1, columnspan=2, sticky="ew", padx=5, pady=(0, 20))

        # Propietario.
        ttk.Label(frm, text="Attuned to:").grid(row=2, column=0, sticky="w", padx=(0, 5), pady=(0, 20))
        vcmd_owner = (frm.register(self.validate_owner), "%P")

        self.entry_owner = ttk.Entry(
            frm,
            textvariable=self.owner_var,
            width=25,
            validate="key",
            validatecommand=vcmd_owner
        )
        self.entry_owner.grid(row=2, column=1, columnspan=2, sticky="ew", padx=(5, 5), pady=(0, 20))

        # Botones.
        ttk.Button(frm, text="Write Crystal", command=self.write_tag).grid(row=3, column=0, columnspan=1, pady=(0, 0), sticky="ew", padx=(0, 2))
        ttk.Button(frm, text="Read Crystal", command=self.read_tag).grid(row=3, column=1, columnspan=2, pady=(0, 0), sticky="ew", padx=(2, 0))
        # ttk.Button(frm, text="Ver Config/Bloqueo", command=self.check_lock).grid(row=2, column=2, pady=10)

        self.uid_var = ttk.StringVar(value="Crystal UID")
        self.uid_label = ttk.Label(
            frm,
            textvariable=self.uid_var,
            style=self.style_label_color,
            bootstyle="info"
        )
        self.uid_label.grid(row=3, column=3, columnspan=1, pady=(0, 0), padx=(5, 0), sticky="s")


    def validate_text(self, new_value):
        return len(new_value) <= 8

    def validate_owner(self, new_value):
        return len(new_value) <= 12

    def choose_color(self):
        color = colorchooser.askcolor()[0]
        if color:
            r, g, b = int(color[0]), int(color[1]), int(color[2])
            self.color_rgb = (r, g, b)
            hexcode = "#%02x%02x%02x" % (r, g, b)
            self.color_hex_var.set(hexcode)
            self.update_color_preview()

    def update_color_preview(self):
        r, g, b = self.color_rgb
        hex_color = "#%02x%02x%02x" % (r, g, b)
        self.style.configure(self.style_label_color, foreground=hex_color)

        if not self.base_image:
            # Fallback...
            return
        
        # Trabajar con RGBA
        img_rgba = self.base_image.copy()
        pixels = img_rgba.load()
        width, height = img_rgba.size
        
        import colorsys
        h_target, s_target, v_target = colorsys.rgb_to_hsv(r/255.0, g/255.0, b/255.0)
        
        for y in range(height):
            for x in range(width):
                pixel = pixels[x, y]
                gray = pixel[0]  # Canal R (en escala de grises da igual cual coger).
                alpha = pixel[3]  # Canal alfa.
                
                v_original = gray / 255.0
                new_r, new_g, new_b = colorsys.hsv_to_rgb(h_target, s_target, v_original)
                
                pixels[x, y] = (
                    int(new_r * 255),
                    int(new_g * 255),
                    int(new_b * 255),
                    alpha  # Mantener transparencia original.
                )
        
        self.photo = ImageTk.PhotoImage(img_rgba)
        self.color_preview.config(image=self.photo)
        self.color_preview.image = self.photo

    ################ WRITE ################

    def write_tag(self):
        print("\n========== INICIANDO ESCRITURA ==========")
        
        # Get color
        text = self.text_var.get()[:8]
        owner = self.owner_var.get()[:12]
        r, g, b = self.color_rgb

        print(f"Color a escribir: RGB({r}, {g}, {b})")
        print(f"Texto a escribir: '{text}' (longitud: {len(text)})")
        print(f"Propietario a escribir: '{owner}' (longitud: {len(owner)})")

        # Generamos las páginas.
        # Página 4: R G B len(text)
        p4 = [r, g, b, len(text)]

        # El texto del preset entre la 5 y la 6.
        text_bytes = [ord(c) for c in text]
        while len(text_bytes) < 8:
            text_bytes.append(0)

        p5 = text_bytes[0:4]
        p6 = text_bytes[4:8]

        # El nombre del propietario entre la 7, 8 y 9 (12 caracteres).
        owner_bytes = [ord(c) for c in owner]
        while len(owner_bytes) < 12:
            owner_bytes.append(0)

        p7 = owner_bytes[0:4]
        p8 = owner_bytes[4:8]
        p9 = owner_bytes[8:12]

        # FIRMA DE LOS DATOS########################
        firma = "ArkaivosKybr"
        firma_bytes = [ord(c) for c in firma]
        # Combinar y firmar.
        data = p4 + p5 + p6
        signed_data = [b ^ f for b, f in zip(data, firma_bytes)]
        # Repartir en páginas.
        p4 = signed_data[0:4]
        p5 = signed_data[4:8]
        p6 = signed_data[8:12]
        ############################################

        max_attempts = 2
        for global_attempt in range(max_attempts):
            try:
                if global_attempt > 0:
                    print(f"\n[GLOBAL] Reintento completo {global_attempt + 1}/{max_attempts}")
                    time.sleep(0.3)
                
                conn = connect_reader()
                
                ntag_write_page(conn, 4, p4)
                ntag_write_page(conn, 5, p5)
                ntag_write_page(conn, 6, p6)
                ntag_write_page(conn, 7, p7)
                ntag_write_page(conn, 8, p8)
                ntag_write_page(conn, 9, p9)
                
                print("========== ESCRITURA COMPLETADA ==========\n")
                
                # Verificación inmediata
                print("========== VERIFICANDO ESCRITURA ==========")
                time.sleep(0.2)
                p4_verify = ntag_read_page(conn, 4)
                p5_verify = ntag_read_page(conn, 5)
                p6_verify = ntag_read_page(conn, 6)
                p7_verify = ntag_read_page(conn, 7)
                p8_verify = ntag_read_page(conn, 8)
                p9_verify = ntag_read_page(conn, 9)
                print(f"Verificación P4: {list(p4_verify)}")
                print(f"Verificación P5: {list(p5_verify)}")
                print(f"Verificación P6: {list(p6_verify)}")
                print(f"Verificación P7: {list(p7_verify)}")
                print(f"Verificación P8: {list(p8_verify)}")
                print(f"Verificación P9: {list(p9_verify)}")
                print("========== VERIFICACIÓN COMPLETADA ==========\n")
                
                messagebox.showinfo("OK", "Cristal Kyber generado correctamente.")
                return
                
            except Exception as e:
                print(f"[GLOBAL] Error en intento {global_attempt + 1}: {e}")
                if global_attempt == max_attempts - 1:
                    messagebox.showerror("Error", str(e))
                    return

    ################ READ ################

    def read_tag(self):
        print("\n========== INICIANDO LECTURA ==========")
        try:
            conn = connect_reader()
            uid = ntag_get_uid(conn)
            print("UID del tag:", toHexString(uid))
            uid_hex = toHexString(uid)
            self.uid_var.set(f"{uid_hex}")
        except Exception as e:
            messagebox.showerror("Error", str(e))
            return

        try:
            p4 = ntag_read_page(conn, 4)
            p5 = ntag_read_page(conn, 5)
            p6 = ntag_read_page(conn, 6)
            p7 = ntag_read_page(conn, 7)
            p8 = ntag_read_page(conn, 8)
            p9 = ntag_read_page(conn, 9)
        except Exception as e:
            messagebox.showerror("Error", str(e))
            return

        # FIRMA DE LOS DATOS########################
        firma = "ArkaivosKybr" # 12 bytes
        firma_bytes = [ord(c) for c in firma] # -> lista de 12 enteros
        signed_data = p4 + p5 + p6 # 12 bytes
        data = [b ^ f for b, f in zip(signed_data, firma_bytes)]
        p4 = data[0:4]
        p5 = data[4:8]
        p6 = data[8:12]
        ############################################

        r, g, b, length = p4
        text_bytes = p5 + p6
        text = "".join(chr(c) for c in text_bytes[:length])

        # Extraer propietario (12 caracteres) - SIN DESCIFRAR.
        owner_bytes = p7 + p8 + p9
        # Buscar el primer byte 0 para determinar longitud real.
        owner_length = 12
        for i, byte in enumerate(owner_bytes):
            if byte == 0:
                owner_length = i
                break
        owner = "".join(chr(c) for c in owner_bytes[:owner_length])

        self.firma_valida.set(0)

        print(f"Color leído: RGB({r}, {g}, {b})")
        print(f"Texto leído: '{text}'")
        print(f"Propietario leído: '{owner}'")
        print("========== LECTURA COMPLETADA ==========\n")

        self.color_rgb = (r, g, b)
        self.color_hex_var.set("#%02x%02x%02x" % (r, g, b))
        self.update_color_preview()

        self.text_var.set(text)
        self.owner_var.set(owner)

    ################ CHECK LOCK ################
    """
    def check_lock(self):
        print("\n========== VERIFICANDO CONFIGURACIÓN ==========")
        try:
            conn = connect_reader()
        except Exception as e:
            messagebox.showerror("Error", str(e))
            return

        try:
            # Página 2: Lock bytes
            p2 = ntag_read_page(conn, 2)
            print(f"Página 2 (Lock bytes): {list(p2)} -> {toHexString(p2)}")
            
            # Página 3: CC (Capability Container)
            p3 = ntag_read_page(conn, 3)
            print(f"Página 3 (CC): {list(p3)} -> {toHexString(p3)}")
            
            # Páginas 40-41: Dynamic lock bytes (NTAG213)
            p40 = ntag_read_page(conn, 40)
            print(f"Página 40 (Dynamic Lock): {list(p40)} -> {toHexString(p40)}")
            
            p41 = ntag_read_page(conn, 41)
            print(f"Página 41 (CFG0): {list(p41)} -> {toHexString(p41)}")
            
            p42 = ntag_read_page(conn, 42)
            print(f"Página 42 (CFG1): {list(p42)} -> {toHexString(p42)}")
            
            # Análisis
            info = "CONFIGURACIÓN DEL TAG:\n\n"
            info += f"Lock bytes (P2): {toHexString(p2)}\n"
            info += f"Capability Container (P3): {toHexString(p3)}\n"
            info += f"Dynamic Lock (P40): {toHexString(p40)}\n"
            info += f"CFG0 (P41): {toHexString(p41)}\n"
            info += f"CFG1 (P42): {toHexString(p42)}\n\n"
            
            # Interpretación de lock bytes
            if p2[2] != 0x00 or p2[3] != 0x00:
                info += "- PÁGINAS BLOQUEADAS detectadas en P2\n"
            if p40[0] != 0x00 or p40[1] != 0x00 or p40[2] != 0x00:
                info += "- PÁGINAS BLOQUEADAS detectadas en P40\n"
            
            # AUTH0 (primer byte de CFG1) - primera página que requiere autenticación
            auth0 = p42[3]
            info += f"\nAUTH0 = {auth0} (0x{auth0:02X})\n"
            if auth0 <= 8:
                info += f"- Las páginas {auth0}-42 requieren autenticación para escritura\n"
            
            print(info)
            messagebox.showinfo("Configuración del Tag", info)
            
        except Exception as e:
            messagebox.showerror("Error", str(e))
            print(f"Error leyendo configuración: {e}")
        
        print("========== VERIFICACIÓN COMPLETADA ==========\n")
    """

#################### MAIN ####################

if __name__ == "__main__":
    root = ttk.Window(themename="darkly") 
    app = App(root)
    root.mainloop()
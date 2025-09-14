import customtkinter as ctk
import requests
import time
import threading

# تنظیم اولیه تم
ctk.set_appearance_mode("dark")
ctk.set_default_color_theme("blue")

esp_ip = 'http://192.168.109.133'


class ESP32ControllerApp(ctk.CTk):

    def __init__(self):
        super().__init__()

        self.title("ESP32 Train Controller")
        self.geometry("900x600")
        try:
            self.state("zoomed")  # ویندوز
        except:
            self.attributes("-zoomed", True)  # لینوکس
        self.configure(fg_color="#1f1f2e")

        # متغیرها
        self.speed_var = ctk.IntVar(value=0)
        self.direction_var = ctk.IntVar(value=1)
        self.motor_status_var = ctk.BooleanVar(value=False)
        self.song_var = ctk.IntVar(value=1)
        self.volume_var = ctk.IntVar(value=15)
        self.theme_mode = ctk.StringVar(value="dark")

        # کنترل بخار
        self.steam_active = False
        self.last_steam_time = 0
        self.steam_timer = None

        self.build_ui()

    def build_ui(self):
        self.grid_columnconfigure((0, 1), weight=1)
        self.grid_rowconfigure((0, 1, 2), weight=1)

        theme_switch = ctk.CTkOptionMenu(self, values=["dark", "light"],
                                         command=self.change_theme, variable=self.theme_mode)
        theme_switch.grid(row=0, column=1, sticky="ne", padx=20, pady=20)

        self.motor_frame = ctk.CTkFrame(self)
        self.motor_frame.grid(row=0, column=0, sticky="nsew", padx=20, pady=20)

        ctk.CTkLabel(self.motor_frame, text="🎛️ Motor Control", font=ctk.CTkFont(size=18, weight="bold")).pack(pady=10)

        ctk.CTkLabel(self.motor_frame, text="Speed").pack()
        self.speed_slider = ctk.CTkSlider(self.motor_frame, from_=0, to=255, variable=self.speed_var,
                                          command=self.on_speed_change)
        self.speed_slider.pack(fill="x", padx=20, pady=10)

        ctk.CTkLabel(self.motor_frame, text="Direction").pack()
        direction_frame = ctk.CTkFrame(self.motor_frame, fg_color="transparent")
        direction_frame.pack()
        ctk.CTkRadioButton(direction_frame, text="Forward", variable=self.direction_var, value=1,
                           command=self.send_motor_update).pack(side="left", padx=10)
        ctk.CTkRadioButton(direction_frame, text="Backward", variable=self.direction_var, value=0,
                           command=self.send_motor_update).pack(side="left", padx=10)

        self.motor_toggle = ctk.CTkSwitch(self.motor_frame, text="Motor Power", variable=self.motor_status_var,
                                          command=self.send_motor_update)
        self.motor_toggle.pack(pady=10)

        self.steam_frame = ctk.CTkFrame(self)
        self.steam_frame.grid(row=1, column=0, sticky="nsew", padx=20, pady=20)

        ctk.CTkLabel(self.steam_frame, text="🌧️ Steam Control", font=ctk.CTkFont(size=18, weight="bold")).pack(pady=10)
        self.steam_btn = ctk.CTkButton(self.steam_frame, text="Activate Steam", command=self.trigger_steam)
        self.steam_btn.pack(pady=10)
        self.steam_status_label = ctk.CTkLabel(self.steam_frame, text="Steam OFF", text_color="gray")
        self.steam_status_label.pack()

        self.music_frame = ctk.CTkFrame(self)
        self.music_frame.grid(row=0, column=1, rowspan=2, sticky="nsew", padx=20, pady=20)

        ctk.CTkLabel(self.music_frame, text="🎵 Music Player", font=ctk.CTkFont(size=18, weight="bold")).pack(pady=10)

        song_frame = ctk.CTkFrame(self.music_frame, fg_color="transparent")
        song_frame.pack(pady=5)
        ctk.CTkButton(song_frame, text="Track 1", command=lambda: self.play_song(1)).pack(side="left", padx=10)
        ctk.CTkButton(song_frame, text="Track 2", command=lambda: self.play_song(2)).pack(side="left", padx=10)

        volume_label = ctk.CTkLabel(self.music_frame, text="Volume")
        volume_label.pack()
        self.volume_slider = ctk.CTkSlider(self.music_frame, from_=0, to=30, variable=self.volume_var,
                                           command=self.set_volume)
        self.volume_slider.pack(fill="x", padx=20, pady=10)

        controls_frame = ctk.CTkFrame(self.music_frame, fg_color="transparent")
        controls_frame.pack(pady=10)
        ctk.CTkButton(controls_frame, text="Pause", command=self.pause_sound).pack(side="left", padx=10)
        ctk.CTkButton(controls_frame, text="Stop", command=self.stop_sound).pack(side="left", padx=10)

        self.status_bar = ctk.CTkLabel(self, text="Ready", anchor="w", fg_color="#2f2f41", height=30)
        self.status_bar.grid(row=2, column=0, columnspan=2, sticky="ew")

    def change_theme(self, mode):
        ctk.set_appearance_mode(mode)

    def on_speed_change(self, val):
        self.send_motor_update()

    def send_motor_update(self):
        try:
            if self.motor_status_var.get():
                speed = self.speed_var.get()
                direction = self.direction_var.get()
                requests.get(f"{esp_ip}/control?speed={speed}&direction={direction}")
                self.update_status(f"Motor running - Speed {speed}, Direction {'Fwd' if direction else 'Bwd'}")
            else:
                requests.get(f"{esp_ip}/control?speed=0")
                self.update_status("Motor stopped")
        except Exception as e:
            self.update_status(f"Motor error: {e}")

    def trigger_steam(self):
        current_time = time.time()
        if self.steam_active or (current_time - self.last_steam_time < 30):
            self.update_status("Steam: Cooldown in effect")
            remaining = int(30 - (current_time - self.last_steam_time))
            self.start_cooldown_timer(remaining)
            return

        self.steam_active = True
        self.last_steam_time = current_time
        self.update_status("Steam activated for 8s")

        def steam_logic():
            try:
                requests.get(f"{esp_ip}/pin15?status=1")
                for i in range(8, 0, -1):
                    self.steam_status_label.configure(text=f"Steam ON - {i}s left", text_color="green")
                    time.sleep(1)
                requests.get(f"{esp_ip}/pin15?status=0")
            except Exception as e:
                self.update_status(f"Steam error: {e}")
            finally:
                self.steam_active = False
                self.start_cooldown_timer(30)

        threading.Thread(target=steam_logic, daemon=True).start()

    def start_cooldown_timer(self, seconds):
        def cooldown():
            for i in range(seconds, 0, -1):
                self.steam_status_label.configure(text=f"Steam Cooldown - {i}s", text_color="red")
                time.sleep(1)
            self.steam_status_label.configure(text="Steam OFF", text_color="gray")

        threading.Thread(target=cooldown, daemon=True).start()

    def play_song(self, num):
        try:
            requests.get(f"{esp_ip}/play?song={num}")
            self.update_status(f"Playing song #{num}")
        except Exception as e:
            self.update_status(f"Play error: {e}")

    def pause_sound(self):
        try:
            requests.get(f"{esp_ip}/pause")
            self.update_status("Paused")
        except Exception as e:
            self.update_status(f"Pause error: {e}")

    def stop_sound(self):
        try:
            requests.get(f"{esp_ip}/stop")
            self.update_status("Stopped")
        except Exception as e:
            self.update_status(f"Stop error: {e}")

    def set_volume(self, value):
        try:
            requests.get(f"{esp_ip}/volume?volume={int(float(value))}")
            self.update_status(f"Volume set to {int(float(value))}")
        except Exception as e:
            self.update_status(f"Volume error: {e}")

    def update_status(self, msg):
        self.status_bar.configure(text=msg)


if __name__ == "__main__":
    app = ESP32ControllerApp()
    app.mainloop()
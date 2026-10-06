"""静态界面文案，运行时不联网；日志内容仍按原有中文翻译规则显示。"""

from __future__ import annotations


KEYS = (
    "app_title", "bridge_port", "refresh_ports", "connect", "disconnect",
    "s3_port", "connect_s3", "disconnect_s3", "current_status",
    "wroom_bluetooth", "wroom_tf", "wroom_memory", "s3_wifi", "cat_mood",
    "wifi_tab", "scan_wifi", "wifi_hint", "network_name", "signal", "security",
    "password", "save_connect", "bluetooth_tab", "refresh_devices", "clear_rescan",
    "reconnect_saved", "play_test_tone", "device_name", "mac", "selected",
    "connect_selected", "service_tab", "deepseek_key", "update_key", "youdao_tts",
    "test_tts_full", "device_group", "refresh_status", "restart_device", "sound_output",
    "bluetooth_mode", "speaker_mode", "save_output_mode", "wroom_log", "s3_log",
    "read_logs", "storage_status", "clear_logs", "save_log", "clear_window",
    "language", "cancel",
)

LANGUAGE_NAMES = {
    "zh-CN": "简体中文", "zh-TW": "繁體中文", "en": "English", "ja": "日本語",
    "ko": "한국어", "fr": "Français", "de": "Deutsch", "es": "Español",
    "pt": "Português", "ru": "Русский", "ar": "العربية", "hi": "हिन्दी",
    "it": "Italiano", "nl": "Nederlands", "tr": "Türkçe", "vi": "Tiếng Việt",
    "th": "ไทย", "id": "Bahasa Indonesia", "ms": "Bahasa Melayu", "pl": "Polski",
}

RTL_LANGUAGES = frozenset({"ar"})

# 每个元组都按 KEYS 的顺序排列，便于测试确保没有漏项。
_ROWS = {
    "zh-CN": (
        "赛博猫猫控制中心", "WROOM 音频桥数据线（蓝牙、TF 卡、播放）", "刷新端口", "连接", "断开",
        "S3 数据线（Wi‑Fi 与对话日志）", "连接 S3", "断开 S3", "当前状态", "WROOM 蓝牙音响", "WROOM TF 卡",
        "WROOM 剩余内存", "S3 Wi‑Fi", "猫猫状态（S3）", "Wi‑Fi", "扫描附近 Wi‑Fi", "双击列表可填入网络名称",
        "网络名称", "信号", "安全", "密码", "保存并连接", "蓝牙音响", "刷新已发现设备", "清除选择并重新扫描",
        "重新连接已保存音响", "播放 3 秒本地测试音（仅 WROOM 蓝牙）", "设备名称", "MAC 地址", "已选择",
        "连接选中的蓝牙音响", "服务与测试", "DeepSeek API Key（发送到 S3，不回显）", "更新 Key",
        "有道语音合成测试", "测试 TTS 全流程", "设备", "刷新状态…", "重启设备…",
        "S3 声音输出（保存后立即生效；播放期间不可切换）", "蓝牙音箱（WROOM）", "直连喇叭（仅 S3）",
        "保存输出模式", "WROOM 日志", "S3 日志", "读取板内日志", "查看存储状态", "清除板内日志",
        "保存日志", "清空窗口", "语言", "取消",
    ),
    "zh-TW": (
        "賽博貓貓控制中心", "WROOM 音訊橋接線（藍牙、TF 卡、播放）", "重新整理連接埠", "連線", "中斷連線",
        "S3 連接線（Wi‑Fi 與對話記錄）", "連線 S3", "中斷 S3", "目前狀態", "WROOM 藍牙喇叭", "WROOM TF 卡",
        "WROOM 剩餘記憶體", "S3 Wi‑Fi", "貓貓狀態（S3）", "Wi‑Fi", "掃描附近 Wi‑Fi", "按兩下清單可填入網路名稱",
        "網路名稱", "訊號", "安全性", "密碼", "儲存並連線", "藍牙喇叭", "重新整理已發現裝置", "清除選擇並重新掃描",
        "重新連線已儲存喇叭", "播放 3 秒本機測試音（僅 WROOM 藍牙）", "裝置名稱", "MAC 位址", "已選取",
        "連線選取的藍牙喇叭", "服務與測試", "DeepSeek API Key（傳送至 S3，不回顯）", "更新 Key",
        "有道語音合成測試", "測試 TTS 完整流程", "裝置", "重新整理狀態…", "重新啟動裝置…",
        "S3 聲音輸出（儲存後立即生效；播放期間不可切換）", "藍牙喇叭（WROOM）", "直連喇叭（僅 S3）",
        "儲存輸出模式", "WROOM 記錄", "S3 記錄", "讀取板內記錄", "查看儲存狀態", "清除板內記錄",
        "儲存記錄", "清空視窗", "語言", "取消",
    ),
    "en": (
        "CyberCat Control Center", "WROOM audio bridge (Bluetooth, TF card, playback)", "Refresh ports", "Connect", "Disconnect",
        "S3 cable (Wi‑Fi and chat logs)", "Connect S3", "Disconnect S3", "Current status", "WROOM Bluetooth speaker", "WROOM TF card",
        "WROOM free memory", "S3 Wi‑Fi", "Cat status (S3)", "Wi‑Fi", "Scan nearby Wi‑Fi", "Double-click a network to fill its name",
        "Network name", "Signal", "Security", "Password", "Save and connect", "Bluetooth speaker", "Refresh discovered devices", "Clear selection and scan again",
        "Reconnect saved speaker", "Play 3-second test sound (WROOM Bluetooth only)", "Device name", "MAC address", "Selected",
        "Connect selected speaker", "Services and tests", "DeepSeek API Key (sent to S3, hidden)", "Update Key",
        "Youdao TTS test", "Test full TTS flow", "Device", "Refresh status…", "Restart device…",
        "S3 audio output (applies immediately; cannot switch during playback)", "Bluetooth speaker (WROOM)", "Direct speaker (S3 only)",
        "Save output mode", "WROOM logs", "S3 logs", "Read device logs", "Storage status", "Clear device logs",
        "Save log", "Clear window", "Language", "Cancel",
    ),
    "ja": (
        "CyberCat コントロールセンター", "WROOM オーディオブリッジ（Bluetooth、TF カード、再生）", "ポートを更新", "接続", "切断",
        "S3 接続（Wi‑Fi と会話ログ）", "S3 に接続", "S3 を切断", "現在の状態", "WROOM Bluetooth スピーカー", "WROOM TF カード",
        "WROOM 空きメモリ", "S3 Wi‑Fi", "猫の状態（S3）", "Wi‑Fi", "近くの Wi‑Fi をスキャン", "一覧をダブルクリックしてネットワーク名を入力",
        "ネットワーク名", "信号", "セキュリティ", "パスワード", "保存して接続", "Bluetooth スピーカー", "検出済みデバイスを更新", "選択を解除して再スキャン",
        "保存済みスピーカーに再接続", "3 秒のテスト音を再生（WROOM Bluetooth のみ）", "デバイス名", "MAC アドレス", "選択済み",
        "選択したスピーカーに接続", "サービスとテスト", "DeepSeek API Key（S3 に送信、非表示）", "Key を更新",
        "有道 TTS テスト", "TTS 全体をテスト", "デバイス", "状態を更新…", "デバイスを再起動…",
        "S3 音声出力（保存後すぐ反映、再生中は切替不可）", "Bluetooth スピーカー（WROOM）", "スピーカー直結（S3 のみ）",
        "出力モードを保存", "WROOM ログ", "S3 ログ", "デバイスログを読み込む", "ストレージ状態", "デバイスログを消去",
        "ログを保存", "ウィンドウをクリア", "言語", "キャンセル",
    ),
    "ko": (
        "CyberCat 제어 센터", "WROOM 오디오 브리지(블루투스, TF 카드, 재생)", "포트 새로 고침", "연결", "연결 해제",
        "S3 케이블(Wi‑Fi 및 대화 로그)", "S3 연결", "S3 연결 해제", "현재 상태", "WROOM 블루투스 스피커", "WROOM TF 카드",
        "WROOM 여유 메모리", "S3 Wi‑Fi", "고양이 상태(S3)", "Wi‑Fi", "주변 Wi‑Fi 검색", "목록을 두 번 클릭해 네트워크 이름 입력",
        "네트워크 이름", "신호", "보안", "비밀번호", "저장 후 연결", "블루투스 스피커", "검색된 기기 새로 고침", "선택 해제 후 다시 검색",
        "저장된 스피커에 다시 연결", "3초 테스트 음 재생(WROOM 블루투스 전용)", "기기 이름", "MAC 주소", "선택됨",
        "선택한 블루투스 스피커 연결", "서비스 및 테스트", "DeepSeek API Key(S3로 전송, 표시 안 함)", "Key 업데이트",
        "유다오 TTS 테스트", "TTS 전체 과정 테스트", "기기", "상태 새로 고침…", "기기 다시 시작…",
        "S3 오디오 출력(저장 후 즉시 적용, 재생 중 전환 불가)", "블루투스 스피커(WROOM)", "스피커 직접 연결(S3 전용)",
        "출력 모드 저장", "WROOM 로그", "S3 로그", "기기 로그 읽기", "저장소 상태", "기기 로그 지우기",
        "로그 저장", "창 비우기", "언어", "취소",
    ),
    "fr": (
        "Centre de contrôle CyberCat", "Pont audio WROOM (Bluetooth, carte TF, lecture)", "Actualiser les ports", "Connecter", "Déconnecter",
        "Câble S3 (Wi‑Fi et journaux de conversation)", "Connecter S3", "Déconnecter S3", "État actuel", "Haut-parleur Bluetooth WROOM", "Carte TF WROOM",
        "Mémoire libre WROOM", "Wi‑Fi S3", "État du chat (S3)", "Wi‑Fi", "Rechercher les Wi‑Fi proches", "Double-cliquez sur un réseau pour remplir son nom",
        "Nom du réseau", "Signal", "Sécurité", "Mot de passe", "Enregistrer et connecter", "Haut-parleur Bluetooth", "Actualiser les appareils détectés", "Effacer la sélection et rescanner",
        "Reconnecter le haut-parleur enregistré", "Lire le son de test de 3 s (Bluetooth WROOM uniquement)", "Nom de l’appareil", "Adresse MAC", "Sélectionné",
        "Connecter le haut-parleur sélectionné", "Services et tests", "Clé API DeepSeek (envoyée au S3, masquée)", "Mettre à jour la clé",
        "Test TTS Youdao", "Tester tout le flux TTS", "Appareil", "Actualiser l’état…", "Redémarrer l’appareil…",
        "Sortie audio S3 (appliquée immédiatement, changement impossible pendant la lecture)", "Haut-parleur Bluetooth (WROOM)", "Haut-parleur direct (S3 uniquement)",
        "Enregistrer le mode de sortie", "Journaux WROOM", "Journaux S3", "Lire les journaux de l’appareil", "État du stockage", "Effacer les journaux de l’appareil",
        "Enregistrer le journal", "Vider la fenêtre", "Langue", "Annuler",
    ),
    "de": (
        "CyberCat-Steuerzentrale", "WROOM-Audiobrücke (Bluetooth, TF-Karte, Wiedergabe)", "Anschlüsse aktualisieren", "Verbinden", "Trennen",
        "S3-Kabel (WLAN und Gesprächsprotokolle)", "S3 verbinden", "S3 trennen", "Aktueller Status", "WROOM-Bluetooth-Lautsprecher", "WROOM-TF-Karte",
        "WROOM freier Speicher", "S3-WLAN", "Katzenstatus (S3)", "WLAN", "WLAN in der Nähe suchen", "Netzwerkname durch Doppelklick übernehmen",
        "Netzwerkname", "Signal", "Sicherheit", "Passwort", "Speichern und verbinden", "Bluetooth-Lautsprecher", "Gefundene Geräte aktualisieren", "Auswahl löschen und erneut suchen",
        "Gespeicherten Lautsprecher erneut verbinden", "3-Sekunden-Testton abspielen (nur WROOM-Bluetooth)", "Gerätename", "MAC-Adresse", "Ausgewählt",
        "Ausgewählten Lautsprecher verbinden", "Dienste und Tests", "DeepSeek-API-Schlüssel (an S3 gesendet, verborgen)", "Schlüssel aktualisieren",
        "Youdao-TTS-Test", "Gesamten TTS-Ablauf testen", "Gerät", "Status aktualisieren…", "Gerät neu starten…",
        "S3-Audioausgabe (sofort aktiv; während der Wiedergabe nicht umschaltbar)", "Bluetooth-Lautsprecher (WROOM)", "Direkter Lautsprecher (nur S3)",
        "Ausgabemodus speichern", "WROOM-Protokolle", "S3-Protokolle", "Geräteprotokolle lesen", "Speicherstatus", "Geräteprotokolle löschen",
        "Protokoll speichern", "Fenster leeren", "Sprache", "Abbrechen",
    ),
    "es": (
        "Centro de control CyberCat", "Puente de audio WROOM (Bluetooth, tarjeta TF, reproducción)", "Actualizar puertos", "Conectar", "Desconectar",
        "Cable S3 (Wi‑Fi y registros de conversación)", "Conectar S3", "Desconectar S3", "Estado actual", "Altavoz Bluetooth WROOM", "Tarjeta TF WROOM",
        "Memoria libre WROOM", "Wi‑Fi S3", "Estado del gato (S3)", "Wi‑Fi", "Buscar redes Wi‑Fi cercanas", "Haz doble clic en una red para rellenar su nombre",
        "Nombre de red", "Señal", "Seguridad", "Contraseña", "Guardar y conectar", "Altavoz Bluetooth", "Actualizar dispositivos detectados", "Borrar selección y volver a buscar",
        "Reconectar el altavoz guardado", "Reproducir tono de prueba de 3 s (solo Bluetooth WROOM)", "Nombre del dispositivo", "Dirección MAC", "Seleccionado",
        "Conectar el altavoz seleccionado", "Servicios y pruebas", "Clave API de DeepSeek (se envía a S3, oculta)", "Actualizar clave",
        "Prueba TTS de Youdao", "Probar todo el flujo TTS", "Dispositivo", "Actualizar estado…", "Reiniciar dispositivo…",
        "Salida de audio S3 (se aplica al guardar; no se cambia durante la reproducción)", "Altavoz Bluetooth (WROOM)", "Altavoz directo (solo S3)",
        "Guardar modo de salida", "Registros WROOM", "Registros S3", "Leer registros del dispositivo", "Estado del almacenamiento", "Borrar registros del dispositivo",
        "Guardar registro", "Limpiar ventana", "Idioma", "Cancelar",
    ),
    "pt": (
        "Central de controle CyberCat", "Ponte de áudio WROOM (Bluetooth, cartão TF, reprodução)", "Atualizar portas", "Conectar", "Desconectar",
        "Cabo S3 (Wi‑Fi e registros de conversa)", "Conectar S3", "Desconectar S3", "Status atual", "Alto-falante Bluetooth WROOM", "Cartão TF WROOM",
        "Memória livre WROOM", "Wi‑Fi S3", "Estado do gato (S3)", "Wi‑Fi", "Buscar redes Wi‑Fi próximas", "Clique duas vezes em uma rede para preencher o nome",
        "Nome da rede", "Sinal", "Segurança", "Senha", "Salvar e conectar", "Alto-falante Bluetooth", "Atualizar dispositivos encontrados", "Limpar seleção e buscar novamente",
        "Reconectar alto-falante salvo", "Tocar som de teste de 3 s (somente Bluetooth WROOM)", "Nome do dispositivo", "Endereço MAC", "Selecionado",
        "Conectar alto-falante selecionado", "Serviços e testes", "Chave de API DeepSeek (enviada ao S3, oculta)", "Atualizar chave",
        "Teste TTS Youdao", "Testar fluxo TTS completo", "Dispositivo", "Atualizar status…", "Reiniciar dispositivo…",
        "Saída de áudio S3 (aplicada ao salvar; não pode mudar durante a reprodução)", "Alto-falante Bluetooth (WROOM)", "Alto-falante direto (somente S3)",
        "Salvar modo de saída", "Registros WROOM", "Registros S3", "Ler registros do dispositivo", "Status do armazenamento", "Limpar registros do dispositivo",
        "Salvar registro", "Limpar janela", "Idioma", "Cancelar",
    ),
    "ru": (
        "Центр управления CyberCat", "Аудиомост WROOM (Bluetooth, TF-карта, воспроизведение)", "Обновить порты", "Подключить", "Отключить",
        "Кабель S3 (Wi‑Fi и журналы диалогов)", "Подключить S3", "Отключить S3", "Текущее состояние", "Bluetooth-колонка WROOM", "TF-карта WROOM",
        "Свободная память WROOM", "Wi‑Fi S3", "Состояние кота (S3)", "Wi‑Fi", "Сканировать ближайшие сети Wi‑Fi", "Дважды щёлкните сеть, чтобы подставить её имя",
        "Имя сети", "Сигнал", "Безопасность", "Пароль", "Сохранить и подключиться", "Bluetooth-колонка", "Обновить найденные устройства", "Сбросить выбор и сканировать снова",
        "Подключить сохранённую колонку повторно", "Воспроизвести тестовый звук 3 с (только Bluetooth WROOM)", "Имя устройства", "MAC-адрес", "Выбрано",
        "Подключить выбранную колонку", "Службы и тесты", "Ключ API DeepSeek (передаётся на S3, скрыт)", "Обновить ключ",
        "Тест TTS Youdao", "Проверить весь процесс TTS", "Устройство", "Обновить состояние…", "Перезапустить устройство…",
        "Аудиовыход S3 (применяется сразу; нельзя переключать во время воспроизведения)", "Bluetooth-колонка (WROOM)", "Прямой динамик (только S3)",
        "Сохранить режим вывода", "Журнал WROOM", "Журнал S3", "Прочитать журналы устройства", "Состояние хранилища", "Очистить журналы устройства",
        "Сохранить журнал", "Очистить окно", "Язык", "Отмена",
    ),
    "ar": (
        "مركز تحكم CyberCat", "جسر الصوت WROOM (Bluetooth وبطاقة TF والتشغيل)", "تحديث المنافذ", "اتصال", "قطع الاتصال",
        "كابل S3 (Wi‑Fi وسجلات المحادثة)", "اتصال بـ S3", "قطع اتصال S3", "الحالة الحالية", "مكبر Bluetooth ‏WROOM", "بطاقة TF ‏WROOM",
        "الذاكرة المتاحة في WROOM", "Wi‑Fi ‏S3", "حالة القطة (S3)", "Wi‑Fi", "البحث عن شبكات Wi‑Fi القريبة", "انقر نقراً مزدوجاً على شبكة لملء اسمها",
        "اسم الشبكة", "الإشارة", "الأمان", "كلمة المرور", "حفظ واتصال", "مكبر Bluetooth", "تحديث الأجهزة المكتشفة", "مسح التحديد وإعادة البحث",
        "إعادة الاتصال بالمكبر المحفوظ", "تشغيل صوت اختبار ٣ ثوانٍ (Bluetooth ‏WROOM فقط)", "اسم الجهاز", "عنوان MAC", "محدد",
        "الاتصال بمكبر Bluetooth المحدد", "الخدمات والاختبارات", "مفتاح DeepSeek API (يُرسل إلى S3 ومخفي)", "تحديث المفتاح",
        "اختبار Youdao TTS", "اختبار مسار TTS بالكامل", "الجهاز", "تحديث الحالة…", "إعادة تشغيل الجهاز…",
        "إخراج الصوت من S3 (يُطبّق فور الحفظ ولا يمكن تغييره أثناء التشغيل)", "مكبر Bluetooth ‏(WROOM)", "مكبر مباشر (S3 فقط)",
        "حفظ وضع الإخراج", "سجلات WROOM", "سجلات S3", "قراءة سجلات الجهاز", "حالة التخزين", "مسح سجلات الجهاز",
        "حفظ السجل", "مسح النافذة", "اللغة", "إلغاء",
    ),
    "hi": (
        "CyberCat नियंत्रण केंद्र", "WROOM ऑडियो ब्रिज (Bluetooth, TF कार्ड, प्लेबैक)", "पोर्ट रीफ़्रेश करें", "कनेक्ट करें", "डिस्कनेक्ट करें",
        "S3 केबल (Wi‑Fi और बातचीत लॉग)", "S3 कनेक्ट करें", "S3 डिस्कनेक्ट करें", "वर्तमान स्थिति", "WROOM Bluetooth स्पीकर", "WROOM TF कार्ड",
        "WROOM की खाली मेमोरी", "S3 Wi‑Fi", "बिल्ली की स्थिति (S3)", "Wi‑Fi", "आस-पास के Wi‑Fi स्कैन करें", "नाम भरने के लिए नेटवर्क पर डबल-क्लिक करें",
        "नेटवर्क नाम", "सिग्नल", "सुरक्षा", "पासवर्ड", "सहेजें और कनेक्ट करें", "Bluetooth स्पीकर", "खोजे गए डिवाइस रीफ़्रेश करें", "चयन हटाएँ और फिर स्कैन करें",
        "सहेजे गए स्पीकर से फिर जुड़ें", "3 सेकंड की टेस्ट ध्वनि चलाएँ (केवल WROOM Bluetooth)", "डिवाइस का नाम", "MAC पता", "चयनित",
        "चयनित Bluetooth स्पीकर से जुड़ें", "सेवाएँ और परीक्षण", "DeepSeek API Key (S3 को भेजी जाती है, छिपी रहती है)", "Key अपडेट करें",
        "Youdao TTS परीक्षण", "पूरा TTS प्रवाह जाँचें", "डिवाइस", "स्थिति रीफ़्रेश करें…", "डिवाइस रीस्टार्ट करें…",
        "S3 ऑडियो आउटपुट (सहेजते ही लागू; प्लेबैक के दौरान नहीं बदल सकता)", "Bluetooth स्पीकर (WROOM)", "सीधा स्पीकर (केवल S3)",
        "आउटपुट मोड सहेजें", "WROOM लॉग", "S3 लॉग", "डिवाइस लॉग पढ़ें", "स्टोरेज स्थिति", "डिवाइस लॉग मिटाएँ",
        "लॉग सहेजें", "विंडो साफ़ करें", "भाषा", "रद्द करें",
    ),
    "it": (
        "Centro di controllo CyberCat", "Bridge audio WROOM (Bluetooth, scheda TF, riproduzione)", "Aggiorna porte", "Connetti", "Disconnetti",
        "Cavo S3 (Wi‑Fi e registri conversazioni)", "Connetti S3", "Disconnetti S3", "Stato attuale", "Altoparlante Bluetooth WROOM", "Scheda TF WROOM",
        "Memoria libera WROOM", "Wi‑Fi S3", "Stato del gatto (S3)", "Wi‑Fi", "Cerca reti Wi‑Fi vicine", "Fai doppio clic su una rete per inserirne il nome",
        "Nome rete", "Segnale", "Sicurezza", "Password", "Salva e connetti", "Altoparlante Bluetooth", "Aggiorna dispositivi trovati", "Cancella selezione e ripeti la scansione",
        "Riconnetti l’altoparlante salvato", "Riproduci tono di prova di 3 s (solo Bluetooth WROOM)", "Nome dispositivo", "Indirizzo MAC", "Selezionato",
        "Connetti l’altoparlante selezionato", "Servizi e test", "Chiave API DeepSeek (inviata a S3, nascosta)", "Aggiorna chiave",
        "Test TTS Youdao", "Prova l’intero flusso TTS", "Dispositivo", "Aggiorna stato…", "Riavvia dispositivo…",
        "Uscita audio S3 (attiva al salvataggio; non modificabile durante la riproduzione)", "Altoparlante Bluetooth (WROOM)", "Altoparlante diretto (solo S3)",
        "Salva modalità di uscita", "Registri WROOM", "Registri S3", "Leggi registri dispositivo", "Stato archiviazione", "Cancella registri dispositivo",
        "Salva registro", "Svuota finestra", "Lingua", "Annulla",
    ),
    "nl": (
        "CyberCat-bedieningscentrum", "WROOM-audiobrug (Bluetooth, TF-kaart, afspelen)", "Poorten vernieuwen", "Verbinden", "Verbinding verbreken",
        "S3-kabel (wifi en gesprekslogboeken)", "S3 verbinden", "S3 verbreken", "Huidige status", "WROOM-Bluetoothspeaker", "WROOM-TF-kaart",
        "Vrij geheugen WROOM", "S3-wifi", "Kattenstatus (S3)", "Wifi", "Wifi in de buurt scannen", "Dubbelklik op een netwerk om de naam in te vullen",
        "Netwerknaam", "Signaal", "Beveiliging", "Wachtwoord", "Opslaan en verbinden", "Bluetoothspeaker", "Gevonden apparaten vernieuwen", "Selectie wissen en opnieuw scannen",
        "Opnieuw verbinden met opgeslagen speaker", "Testtoon van 3 seconden afspelen (alleen WROOM-Bluetooth)", "Apparaatnaam", "MAC-adres", "Geselecteerd",
        "Verbinden met geselecteerde speaker", "Services en tests", "DeepSeek API-sleutel (naar S3 verzonden, verborgen)", "Sleutel bijwerken",
        "Youdao-TTS-test", "Volledige TTS-stroom testen", "Apparaat", "Status vernieuwen…", "Apparaat opnieuw starten…",
        "S3-audio-uitvoer (direct actief na opslaan; niet wijzigbaar tijdens afspelen)", "Bluetoothspeaker (WROOM)", "Directe speaker (alleen S3)",
        "Uitvoermodus opslaan", "WROOM-logboeken", "S3-logboeken", "Apparaatlogboeken lezen", "Opslagstatus", "Apparaatlogboeken wissen",
        "Logboek opslaan", "Venster wissen", "Taal", "Annuleren",
    ),
    "tr": (
        "CyberCat Kontrol Merkezi", "WROOM ses köprüsü (Bluetooth, TF kartı, oynatma)", "Bağlantı noktalarını yenile", "Bağlan", "Bağlantıyı kes",
        "S3 kablosu (Wi‑Fi ve konuşma günlükleri)", "S3'e bağlan", "S3 bağlantısını kes", "Geçerli durum", "WROOM Bluetooth hoparlörü", "WROOM TF kartı",
        "WROOM boş bellek", "S3 Wi‑Fi", "Kedi durumu (S3)", "Wi‑Fi", "Yakındaki Wi‑Fi ağlarını tara", "Ağ adını doldurmak için listeye çift tıklayın",
        "Ağ adı", "Sinyal", "Güvenlik", "Parola", "Kaydet ve bağlan", "Bluetooth hoparlörü", "Bulunan aygıtları yenile", "Seçimi temizle ve yeniden tara",
        "Kaydedilen hoparlöre yeniden bağlan", "3 saniyelik test sesi çal (yalnızca WROOM Bluetooth)", "Aygıt adı", "MAC adresi", "Seçildi",
        "Seçili hoparlöre bağlan", "Hizmetler ve testler", "DeepSeek API Anahtarı (S3'e gönderilir, gizlidir)", "Anahtarı güncelle",
        "Youdao TTS testi", "Tüm TTS akışını test et", "Aygıt", "Durumu yenile…", "Aygıtı yeniden başlat…",
        "S3 ses çıkışı (kaydedince hemen uygulanır; oynatma sırasında değiştirilemez)", "Bluetooth hoparlörü (WROOM)", "Doğrudan hoparlör (yalnızca S3)",
        "Çıkış modunu kaydet", "WROOM günlükleri", "S3 günlükleri", "Aygıt günlüklerini oku", "Depolama durumu", "Aygıt günlüklerini temizle",
        "Günlüğü kaydet", "Pencereyi temizle", "Dil", "İptal",
    ),
    "vi": (
        "Trung tâm điều khiển CyberCat", "Cầu nối âm thanh WROOM (Bluetooth, thẻ TF, phát lại)", "Làm mới cổng", "Kết nối", "Ngắt kết nối",
        "Cáp S3 (Wi‑Fi và nhật ký hội thoại)", "Kết nối S3", "Ngắt kết nối S3", "Trạng thái hiện tại", "Loa Bluetooth WROOM", "Thẻ TF WROOM",
        "Bộ nhớ trống WROOM", "Wi‑Fi S3", "Trạng thái mèo (S3)", "Wi‑Fi", "Quét Wi‑Fi lân cận", "Nhấp đúp vào mạng để điền tên",
        "Tên mạng", "Tín hiệu", "Bảo mật", "Mật khẩu", "Lưu và kết nối", "Loa Bluetooth", "Làm mới thiết bị đã tìm thấy", "Xóa lựa chọn và quét lại",
        "Kết nối lại loa đã lưu", "Phát âm thử 3 giây (chỉ Bluetooth WROOM)", "Tên thiết bị", "Địa chỉ MAC", "Đã chọn",
        "Kết nối loa Bluetooth đã chọn", "Dịch vụ và kiểm tra", "Khóa API DeepSeek (gửi đến S3, được ẩn)", "Cập nhật khóa",
        "Kiểm tra TTS Youdao", "Kiểm tra toàn bộ quy trình TTS", "Thiết bị", "Làm mới trạng thái…", "Khởi động lại thiết bị…",
        "Đầu ra âm thanh S3 (có hiệu lực ngay khi lưu; không thể đổi khi đang phát)", "Loa Bluetooth (WROOM)", "Loa trực tiếp (chỉ S3)",
        "Lưu chế độ đầu ra", "Nhật ký WROOM", "Nhật ký S3", "Đọc nhật ký thiết bị", "Trạng thái lưu trữ", "Xóa nhật ký thiết bị",
        "Lưu nhật ký", "Xóa cửa sổ", "Ngôn ngữ", "Hủy",
    ),
    "th": (
        "ศูนย์ควบคุม CyberCat", "บริดจ์เสียง WROOM (Bluetooth, การ์ด TF, เล่นเสียง)", "รีเฟรชพอร์ต", "เชื่อมต่อ", "ตัดการเชื่อมต่อ",
        "สาย S3 (Wi‑Fi และบันทึกการสนทนา)", "เชื่อมต่อ S3", "ตัดการเชื่อมต่อ S3", "สถานะปัจจุบัน", "ลำโพง Bluetooth WROOM", "การ์ด TF WROOM",
        "หน่วยความจำว่าง WROOM", "Wi‑Fi S3", "สถานะแมว (S3)", "Wi‑Fi", "สแกน Wi‑Fi ใกล้เคียง", "ดับเบิลคลิกเครือข่ายเพื่อกรอกชื่อ",
        "ชื่อเครือข่าย", "สัญญาณ", "ความปลอดภัย", "รหัสผ่าน", "บันทึกและเชื่อมต่อ", "ลำโพง Bluetooth", "รีเฟรชอุปกรณ์ที่พบ", "ล้างรายการที่เลือกแล้วสแกนใหม่",
        "เชื่อมต่อลำโพงที่บันทึกไว้อีกครั้ง", "เล่นเสียงทดสอบ 3 วินาที (Bluetooth WROOM เท่านั้น)", "ชื่ออุปกรณ์", "ที่อยู่ MAC", "เลือกแล้ว",
        "เชื่อมต่อลำโพงที่เลือก", "บริการและการทดสอบ", "DeepSeek API Key (ส่งไปยัง S3 และซ่อนไว้)", "อัปเดต Key",
        "ทดสอบ Youdao TTS", "ทดสอบขั้นตอน TTS ทั้งหมด", "อุปกรณ์", "รีเฟรชสถานะ…", "รีสตาร์ทอุปกรณ์…",
        "เอาต์พุตเสียง S3 (มีผลทันทีเมื่อบันทึก; เปลี่ยนระหว่างเล่นไม่ได้)", "ลำโพง Bluetooth (WROOM)", "ลำโพงโดยตรง (S3 เท่านั้น)",
        "บันทึกโหมดเอาต์พุต", "บันทึก WROOM", "บันทึก S3", "อ่านบันทึกอุปกรณ์", "สถานะพื้นที่จัดเก็บ", "ลบบันทึกอุปกรณ์",
        "บันทึกไฟล์บันทึก", "ล้างหน้าต่าง", "ภาษา", "ยกเลิก",
    ),
    "id": (
        "Pusat Kontrol CyberCat", "Jembatan audio WROOM (Bluetooth, kartu TF, pemutaran)", "Segarkan port", "Hubungkan", "Putuskan",
        "Kabel S3 (Wi‑Fi dan log percakapan)", "Hubungkan S3", "Putuskan S3", "Status saat ini", "Speaker Bluetooth WROOM", "Kartu TF WROOM",
        "Memori kosong WROOM", "Wi‑Fi S3", "Suasana kucing (S3)", "Wi‑Fi", "Pindai Wi‑Fi di sekitar", "Klik dua kali jaringan untuk mengisi namanya",
        "Nama jaringan", "Sinyal", "Keamanan", "Kata sandi", "Simpan dan hubungkan", "Speaker Bluetooth", "Segarkan perangkat yang ditemukan", "Hapus pilihan dan pindai ulang",
        "Hubungkan kembali speaker tersimpan", "Putar nada uji 3 detik (khusus Bluetooth WROOM)", "Nama perangkat", "Alamat MAC", "Dipilih",
        "Hubungkan speaker yang dipilih", "Layanan dan pengujian", "Kunci API DeepSeek (dikirim ke S3, disembunyikan)", "Perbarui kunci",
        "Uji TTS Youdao", "Uji seluruh alur TTS", "Perangkat", "Segarkan status…", "Mulai ulang perangkat…",
        "Keluaran audio S3 (langsung berlaku setelah disimpan; tidak dapat diganti saat diputar)", "Speaker Bluetooth (WROOM)", "Speaker langsung (khusus S3)",
        "Simpan mode keluaran", "Log WROOM", "Log S3", "Baca log perangkat", "Status penyimpanan", "Hapus log perangkat",
        "Simpan log", "Kosongkan jendela", "Bahasa", "Batal",
    ),
    "ms": (
        "Pusat Kawalan CyberCat", "Jambatan audio WROOM (Bluetooth, kad TF, main balik)", "Segar semula port", "Sambung", "Putuskan sambungan",
        "Kabel S3 (Wi‑Fi dan log perbualan)", "Sambung S3", "Putuskan S3", "Status semasa", "Pembesar suara Bluetooth WROOM", "Kad TF WROOM",
        "Memori kosong WROOM", "Wi‑Fi S3", "Keadaan kucing (S3)", "Wi‑Fi", "Imbas Wi‑Fi berdekatan", "Klik dua kali rangkaian untuk mengisi namanya",
        "Nama rangkaian", "Isyarat", "Keselamatan", "Kata laluan", "Simpan dan sambung", "Pembesar suara Bluetooth", "Segar semula peranti ditemui", "Kosongkan pilihan dan imbas semula",
        "Sambung semula pembesar suara tersimpan", "Mainkan nada ujian 3 saat (Bluetooth WROOM sahaja)", "Nama peranti", "Alamat MAC", "Dipilih",
        "Sambung pembesar suara terpilih", "Perkhidmatan dan ujian", "Kunci API DeepSeek (dihantar ke S3, disembunyikan)", "Kemas kini kunci",
        "Ujian TTS Youdao", "Uji aliran TTS penuh", "Peranti", "Segar semula status…", "Mulakan semula peranti…",
        "Output audio S3 (berkuat kuasa selepas disimpan; tidak boleh ditukar semasa main balik)", "Pembesar suara Bluetooth (WROOM)", "Pembesar suara terus (S3 sahaja)",
        "Simpan mod output", "Log WROOM", "Log S3", "Baca log peranti", "Status storan", "Kosongkan log peranti",
        "Simpan log", "Kosongkan tetingkap", "Bahasa", "Batal",
    ),
    "pl": (
        "Centrum sterowania CyberCat", "Most audio WROOM (Bluetooth, karta TF, odtwarzanie)", "Odśwież porty", "Połącz", "Rozłącz",
        "Przewód S3 (Wi‑Fi i dzienniki rozmów)", "Połącz S3", "Rozłącz S3", "Bieżący stan", "Głośnik Bluetooth WROOM", "Karta TF WROOM",
        "Wolna pamięć WROOM", "Wi‑Fi S3", "Nastrój kota (S3)", "Wi‑Fi", "Skanuj pobliskie sieci Wi‑Fi", "Kliknij dwukrotnie sieć, aby wstawić jej nazwę",
        "Nazwa sieci", "Sygnał", "Zabezpieczenia", "Hasło", "Zapisz i połącz", "Głośnik Bluetooth", "Odśwież wykryte urządzenia", "Wyczyść wybór i skanuj ponownie",
        "Połącz ponownie zapisany głośnik", "Odtwórz 3-sekundowy dźwięk testowy (tylko Bluetooth WROOM)", "Nazwa urządzenia", "Adres MAC", "Wybrano",
        "Połącz wybrany głośnik", "Usługi i testy", "Klucz API DeepSeek (wysyłany do S3, ukryty)", "Aktualizuj klucz",
        "Test TTS Youdao", "Testuj cały przepływ TTS", "Urządzenie", "Odśwież stan…", "Uruchom ponownie urządzenie…",
        "Wyjście audio S3 (działa od razu po zapisaniu; bez przełączania podczas odtwarzania)", "Głośnik Bluetooth (WROOM)", "Głośnik bezpośredni (tylko S3)",
        "Zapisz tryb wyjścia", "Dzienniki WROOM", "Dzienniki S3", "Odczytaj dzienniki urządzenia", "Stan pamięci", "Wyczyść dzienniki urządzenia",
        "Zapisz dziennik", "Wyczyść okno", "Język", "Anuluj",
    ),
}

_STATE_KEYS = (
    "connected", "disconnected", "connecting", "not_selected", "mounted", "not_detected", "unknown",
    "booting", "standby", "recognizing", "transcribing", "ai_reply", "tts", "no_command", "backend_error", "offline_command",
)
_STATE_ROWS = {
    "zh-CN": ("已连接", "未连接", "正在连接", "未选择", "已挂载", "未检测到", "未知", "正在启动", "待机中", "正在识别语音", "正在转文字", "AI 正在回复", "正在合成语音", "没有识别到指令", "网络或服务异常", "正在执行离线指令"),
    "zh-TW": ("已連線", "未連線", "正在連線", "未選取", "已掛載", "未偵測到", "未知", "正在啟動", "待機中", "正在辨識語音", "正在轉成文字", "AI 正在回覆", "正在合成語音", "沒有辨識到指令", "網路或服務異常", "正在執行離線指令"),
    "en": ("Connected", "Not connected", "Connecting", "Not selected", "Mounted", "Not detected", "Unknown", "Starting", "Standby", "Recognizing speech", "Transcribing speech", "AI is replying", "Synthesizing speech", "No command recognized", "Network or service error", "Running offline command"),
    "ja": ("接続済み", "未接続", "接続中", "未選択", "マウント済み", "未検出", "不明", "起動中", "待機中", "音声を認識中", "文字に変換中", "AI が返信中", "音声を合成中", "コマンドを認識できません", "ネットワークまたはサービスのエラー", "オフラインコマンドを実行中"),
    "ko": ("연결됨", "연결 안 됨", "연결 중", "선택 안 됨", "마운트됨", "감지되지 않음", "알 수 없음", "시작 중", "대기 중", "음성 인식 중", "텍스트 변환 중", "AI 응답 중", "음성 합성 중", "명령을 인식하지 못함", "네트워크 또는 서비스 오류", "오프라인 명령 실행 중"),
    "fr": ("Connecté", "Non connecté", "Connexion…", "Non sélectionné", "Monté", "Non détecté", "Inconnu", "Démarrage", "En veille", "Reconnaissance vocale", "Transcription", "Réponse de l’IA", "Synthèse vocale", "Commande non reconnue", "Erreur réseau ou service", "Commande hors ligne en cours"),
    "de": ("Verbunden", "Nicht verbunden", "Verbindung…", "Nicht ausgewählt", "Eingebunden", "Nicht erkannt", "Unbekannt", "Startet", "Bereit", "Spracherkennung", "Sprach-zu-Text", "KI antwortet", "Sprachausgabe", "Kein Befehl erkannt", "Netzwerk- oder Dienstfehler", "Offline-Befehl wird ausgeführt"),
    "es": ("Conectado", "Desconectado", "Conectando", "No seleccionado", "Montado", "No detectado", "Desconocido", "Iniciando", "En espera", "Reconociendo voz", "Transcribiendo", "La IA está respondiendo", "Sintetizando voz", "No se reconoció ningún comando", "Error de red o servicio", "Ejecutando comando sin conexión"),
    "pt": ("Conectado", "Desconectado", "Conectando", "Não selecionado", "Montado", "Não detectado", "Desconhecido", "Iniciando", "Em espera", "Reconhecendo fala", "Transcrevendo fala", "IA respondendo", "Sintetizando fala", "Nenhum comando reconhecido", "Erro de rede ou serviço", "Executando comando offline"),
    "ru": ("Подключено", "Не подключено", "Подключение", "Не выбрано", "Подключено", "Не обнаружено", "Неизвестно", "Запуск", "Ожидание", "Распознавание речи", "Преобразование в текст", "ИИ отвечает", "Синтез речи", "Команда не распознана", "Ошибка сети или службы", "Выполняется автономная команда"),
    "ar": ("متصل", "غير متصل", "جارٍ الاتصال", "غير محدد", "تم التركيب", "لم يتم الكشف", "غير معروف", "جارٍ التشغيل", "استعداد", "جارٍ التعرف على الكلام", "جارٍ تحويل الكلام إلى نص", "يرد الذكاء الاصطناعي", "جارٍ تركيب الكلام", "لم يتم التعرف على الأمر", "خطأ في الشبكة أو الخدمة", "جارٍ تنفيذ أمر دون اتصال"),
    "hi": ("कनेक्ट है", "कनेक्ट नहीं है", "कनेक्ट हो रहा है", "चयनित नहीं", "माउंट किया गया", "पता नहीं चला", "अज्ञात", "शुरू हो रहा है", "स्टैंडबाय", "आवाज़ पहचान रहा है", "टेक्स्ट में बदल रहा है", "AI जवाब दे रहा है", "आवाज़ बना रहा है", "कोई कमांड नहीं पहचानी", "नेटवर्क या सेवा त्रुटि", "ऑफ़लाइन कमांड चल रही है"),
    "it": ("Connesso", "Non connesso", "Connessione…", "Non selezionato", "Montato", "Non rilevato", "Sconosciuto", "Avvio", "In attesa", "Riconoscimento vocale", "Trascrizione", "L’IA sta rispondendo", "Sintesi vocale", "Nessun comando riconosciuto", "Errore di rete o servizio", "Comando offline in esecuzione"),
    "nl": ("Verbonden", "Niet verbonden", "Verbinden…", "Niet geselecteerd", "Gekoppeld", "Niet gedetecteerd", "Onbekend", "Opstarten", "Stand-by", "Spraak herkennen", "Omzetten naar tekst", "AI antwoordt", "Spraak synthetiseren", "Geen opdracht herkend", "Netwerk- of servicefout", "Offline opdracht uitvoeren"),
    "tr": ("Bağlı", "Bağlı değil", "Bağlanıyor", "Seçilmedi", "Bağlandı", "Algılanmadı", "Bilinmiyor", "Başlatılıyor", "Beklemede", "Konuşma tanınıyor", "Metne dönüştürülüyor", "Yapay zekâ yanıtlıyor", "Konuşma sentezleniyor", "Komut tanınmadı", "Ağ veya hizmet hatası", "Çevrimdışı komut yürütülüyor"),
    "vi": ("Đã kết nối", "Chưa kết nối", "Đang kết nối", "Chưa chọn", "Đã gắn", "Không phát hiện", "Không rõ", "Đang khởi động", "Chờ", "Đang nhận dạng giọng nói", "Đang chuyển thành văn bản", "AI đang trả lời", "Đang tổng hợp giọng nói", "Không nhận diện được lệnh", "Lỗi mạng hoặc dịch vụ", "Đang chạy lệnh ngoại tuyến"),
    "th": ("เชื่อมต่อแล้ว", "ยังไม่เชื่อมต่อ", "กำลังเชื่อมต่อ", "ยังไม่ได้เลือก", "เมานต์แล้ว", "ไม่พบ", "ไม่ทราบ", "กำลังเริ่มต้น", "สแตนด์บาย", "กำลังรู้จำเสียง", "กำลังแปลงเป็นข้อความ", "AI กำลังตอบ", "กำลังสังเคราะห์เสียง", "ไม่รู้จำคำสั่ง", "เครือข่ายหรือบริการขัดข้อง", "กำลังทำคำสั่งออฟไลน์"),
    "id": ("Terhubung", "Tidak terhubung", "Menghubungkan", "Belum dipilih", "Terpasang", "Tidak terdeteksi", "Tidak diketahui", "Memulai", "Siaga", "Mengenali suara", "Mengubah ke teks", "AI sedang membalas", "Menyintesis suara", "Perintah tidak dikenali", "Kesalahan jaringan atau layanan", "Menjalankan perintah offline"),
    "ms": ("Disambungkan", "Tidak disambungkan", "Sedang menyambung", "Belum dipilih", "Dipasang", "Tidak dikesan", "Tidak diketahui", "Sedang bermula", "Sedia", "Mengecam pertuturan", "Menukar kepada teks", "AI sedang membalas", "Mensintesis pertuturan", "Arahan tidak dikenal pasti", "Ralat rangkaian atau perkhidmatan", "Menjalankan arahan luar talian"),
    "pl": ("Połączono", "Niepołączono", "Łączenie…", "Nie wybrano", "Zamontowano", "Nie wykryto", "Nieznany", "Uruchamianie", "Gotowość", "Rozpoznawanie mowy", "Transkrypcja", "AI odpowiada", "Synteza mowy", "Nie rozpoznano polecenia", "Błąd sieci lub usługi", "Wykonywanie polecenia offline"),
}

CATALOGS = {locale: dict(zip(KEYS, row, strict=True)) for locale, row in _ROWS.items()}
STATE_CATALOGS = {locale: dict(zip(_STATE_KEYS, row, strict=True)) for locale, row in _STATE_ROWS.items()}
SOURCE_TO_KEY = {value: key for key, value in CATALOGS["zh-CN"].items()}
SOURCE_TO_KEY["赛博猫猫 · 双板 USB 管理器"] = "app_title"
SOURCE_TO_KEY["🐾 赛博猫猫控制中心"] = "app_title"
LANGUAGE_TO_CODE = {name: code for code, name in LANGUAGE_NAMES.items()}


def translate(text: str, locale: str) -> str:
    """Translate a known UI label; leave user data, protocol text, and logs untouched."""
    key = SOURCE_TO_KEY.get(text)
    if key is None:
        return text
    return CATALOGS.get(locale, CATALOGS["zh-CN"]).get(key, text)


def is_rtl(locale: str) -> bool:
    return locale in RTL_LANGUAGES


_MOOD_STATE_KEYS = {
    "boot_off": "booting", "idle_blue": "standby", "recognizing_yellow": "recognizing",
    "speech_to_text_cyan": "transcribing", "ai_reply_purple": "ai_reply",
    "text_to_speech_green": "tts", "recognition_red": "no_command",
    "backend_error_blink": "backend_error", "backend_red": "backend_error",
    "backend_green": "backend_error", "fixed_pink": "offline_command", "unknown": "unknown",
}


def state_text(key: str, locale: str) -> str:
    return STATE_CATALOGS.get(locale, STATE_CATALOGS["zh-CN"]).get(key, key)


def mood_text(mood: str, locale: str) -> str:
    key = _MOOD_STATE_KEYS.get(mood)
    return state_text(key, locale) if key else mood


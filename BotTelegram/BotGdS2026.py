import os
import telebot
import requests
from datetime import datetime
from dotenv import load_dotenv
from telebot import types

# Carica le variabili dal file .env
load_dotenv()

# Sostituisci con i tuoi dati
#TOKEN = ''
#CHANNEL_ID = '3203205'
#READ_API_KEY = ''


TOKEN = os.getenv('TELEGRAM_TOKEN')
CHANNEL_ID = os.getenv('THINGSPEAK_CHANNEL_ID')
READ_API_KEY = os.getenv('THINGSPEAK_READ_KEY')

bot = telebot.TeleBot(TOKEN)

# Funzione per leggere la temperatura cpu Raspberry
def get_cpu_temp():
    # Esegue il comando di sistema e legge l'output
    temp = os.popen("vcgencmd measure_temp").readline()
    return temp.replace("temp=", "").replace("'C\n", "°C")

# Funzione per leggere la RAM
def get_ram_info():
    # Esegue 'free -m' e recupera la seconda riga (quella della Mem)
    p = os.popen('free -m')
    lines = p.readlines()
    # lines[1] è la riga con i dati della memoria
    row = lines[1].split()
    total = row[1]
    used = row[2]
    free = row[3]
    return f"Totale: {total}MB\nUsata: {used}MB\nLibera: {free}MB"


@bot.message_handler(commands=['start', 'aiuto'])
@bot.message_handler(commands=['start', 'aiuto'])
def send_welcome(message):
    # Creazione della tastiera (resize_keyboard la rende più piccola e pulita)
    markup = types.ReplyKeyboardMarkup(resize_keyboard=True, row_width=2)

    # Definizione dei tasti
    #btn_dati = types.KeyboardButton('📊 /dati')
    #btn_cpu = types.KeyboardButton('🌡️ /cputemp')
    #btn_ram = types.KeyboardButton('📟 /ram')

    btn_dati = types.KeyboardButton('📊 Dati')
    btn_cpu = types.KeyboardButton('🌡️ CPU Temp')
    btn_ram = types.KeyboardButton('📟 RAM')

    # Organizzazione dei tasti: /dati a tutta larghezza, gli altri due sotto affiancati
    markup.add(btn_dati)
    markup.row(btn_cpu, btn_ram)

    bot.reply_to(
        message, 
        "🌿 *Benvenuto nel Bot GdS2026!*\nUsa i tasti qui sotto per monitorare i sensori ambientali e lo stato del Raspberry.",
        reply_markup=markup,
        parse_mode='Markdown'
    )

#@bot.message_handler(commands=['dati'])
@bot.message_handler(commands=['dati'], func=lambda message: True)
@bot.message_handler(func=lambda message: message.text == "📊 Dati")
def get_data(message):
    # URL per leggere l'ultimo feed del canale
    url = f"https://api.thingspeak.com/channels/{CHANNEL_ID}/feeds.json?api_key={READ_API_KEY}&results=1"

    try:
        response = requests.get(url).json()
        ultimo_feed = response['feeds'][0]

        # Recupero dati esistenti
        temp = ultimo_feed.get('field1', 'N/D')
        umid = ultimo_feed.get('field2', 'N/D')

        # Recupero nuovi dati sul particolato
        pm1 = ultimo_feed.get('field3', 'N/D')
        pm25 = ultimo_feed.get('field4', 'N/D')
        pm10 = ultimo_feed.get('field5', 'N/D')

        # Formattazione data
        data_iso = ultimo_feed['created_at']
        data_obj = datetime.strptime(data_iso, '%Y-%m-%dT%H:%M:%SZ')
        orario = data_obj.strftime('%H:%M del %d/%m/%Y')

        # Costruzione del messaggio con i nuovi campi
        risposta = "📊 *Rilevazione Sensori*\n"
        risposta += "----------------------------\n"
        risposta += f"🌡️ *Temperatura:* {temp} °C\n"
        risposta += f"💧 *Umidità:* {umid} %\n"
        risposta += "----------------------------\n"
        risposta += "🌬️ *Qualità dell'Aria (Particolato):*\n"
        risposta += f"🔹 *PM 1:* {pm1} µg/m³\n"
        risposta += f"🔹 *PM 2.5:* {pm25} µg/m³\n"
        risposta += f"🔹 *PM 10:* {pm10} µg/m³\n"
        risposta += "----------------------------\n"
        risposta += f"🕒 _Dati aggiornati alle {orario}_"

        bot.reply_to(message, risposta, parse_mode='Markdown')
    except Exception as e:
        bot.reply_to(message, "❌ Errore nel recupero dati. Verifica la connessione o i permessi del canale.")

#@bot.message_handler(commands=['cputemp'])
@bot.message_handler(commands=['cputemp'])
@bot.message_handler(func=lambda message: message.text == "🌡️ CPU Temp")
def send_temp(message):
    try:
        cpu_temp = get_cpu_temp()
        bot.reply_to(message, f"🌡️ Temperatura CPU: {cpu_temp}")
    except Exception as e:
        bot.reply_to(message, "Errore nella lettura della temperatura.")

#@bot.message_handler(commands=['ram']):
@bot.message_handler(commands=['ram'])
@bot.message_handler(func=lambda message: message.text == "📟 RAM")
def send_ram(message):
    try:
        ram_status = get_ram_info()
        bot.reply_to(message, f"📊 Stato RAM:\n{ram_status}")
    except Exception as e:
        bot.reply_to(message, "Errore nella lettura della RAM.")

# Avvia il bot
print("Bot in ascolto...")
bot.infinity_polling()

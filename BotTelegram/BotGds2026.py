import os
import telebot
import requests
from datetime import datetime
from dotenv import load_dotenv

# Carica le variabili dal file .env
load_dotenv()

TOKEN = os.getenv('TELEGRAM_TOKEN')
CHANNEL_ID = os.getenv('THINGSPEAK_CHANNEL_ID')
READ_API_KEY = os.getenv('THINGSPEAK_READ_KEY')

bot = telebot.TeleBot(TOKEN)

@bot.message_handler(commands=['start', 'aiuto'])
def send_welcome(message):
    bot.reply_to(message, "🌿 Ciao! Usa il comando /dati per leggere l'ultimo aggiornamento dai sensori ambientali registrato su *ThingSpeak*.")

@bot.message_handler(commands=['dati'])
def get_data(message):
    # URL per leggere l'ultimo feed del canale
    url = f"https://api.thingspeak.com/channels/{CHANNEL_ID}/feeds.json?api_key={READ_API_KEY}&results=1"
    
    try:
        response = requests.get(url).json()
        ultimo_feed = response['feeds'][0]
        
        # Supponendo che il dato sia nel field1
        # valore = ultimo_feed['field1']
        # data_ora = ultimo_feed['created_at']
        
        temp = ultimo_feed.get('field1', 'N/D')
        umid = ultimo_feed.get('field2', 'N/D')

        # Formattazione data
        data_iso = ultimo_feed['created_at']
        data_obj = datetime.strptime(data_iso, '%Y-%m-%dT%H:%M:%SZ')
        orario = data_obj.strftime('%H:%M del %d/%m/%Y')

        # Costruzione del messaggio
        risposta = "📊 *Rilevazione Sensori*\n"
        risposta += "----------------------------\n"
        risposta += f"🌡️ *Temperatura:* {temp} °C\n"
        risposta += f"💧 *Umidità:* {umid} %\n"
        risposta += "----------------------------\n"
        risposta += f"🕒 _Dati aggiornati alle {orario}_"
        
        # testo_risposta = f"📊 *Ultimo Dato:*\nValore: {valore}\nRicevuto il: {data_ora}"
        bot.reply_to(message, risposta, parse_mode='Markdown')
    except Exception as e:
        bot.reply_to(message, "❌ Errore nel recupero dati. Verifica che il canale sia pubblico o che la chiave sia corretta.")

# Avvia il bot
print("Bot in ascolto...")
bot.infinity_polling()
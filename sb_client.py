from supabase import create_client

import streamlit as st
import pandas as pd
import plotly.express as px

import websocket
import threading
import json

st.set_page_config(page_title='Dashboard', layout='centered', initial_sidebar_state='collapsed')

def on_message(ws, message):
    bandJson = json.loads(message)
    print(f'[ESP] Band amplitude measurments: {bandJson}')
    
def on_close(ws, close_status_code, close_msg):
    print("[ESP] CLOSED")

# Init websocketApp
@st.cache_resource
def init_websocket():
    # Init websocket
    wapp = websocket.WebSocketApp("ws://10.0.1.75", on_message=on_message, on_close=on_close)
    return wapp 

wapp = init_websocket()

@st.cache_resource
def init_connection():
    url = st.secrets["SUPABASE_URL"]
    key = st.secrets["SUPABASE_KEY"]
    return create_client(url, key)

supabase = init_connection()

@st.cache_data(ttl=60)
def get_plays_data(table_name: str):
    data = supabase.table(table_name).select('*').execute().data
    df = pd.DataFrame(data)
    df['created_at'] = df['created_at'].apply(lambda x: x.split('.')[0])
    df['created_at'] = pd.to_datetime(df['created_at'])
    
    df['ses_start'] = df['created_at'] - pd.to_timedelta(df['duration'], unit='s')
    df['ses_end'] = df['created_at']
    
    df = df.drop(columns=['created_at'])
    return df[['id', 'ses_start', 'ses_end', 'duration', 'is_music']]

# Supabase data get
df = get_plays_data('playTable')

st.markdown('# Usage history')

df_24h = df[df['ses_start'] > pd.to_datetime('now') - pd.to_timedelta(1, unit='d')]
df_cumsum = df_24h[['ses_end', 'duration', 'is_music']].set_index('ses_end').sort_index()
df_cumsum['duration_cumsum'] = df_cumsum.groupby('is_music')['duration'].cumsum()

fig = px.line(df_cumsum, x=df_cumsum.index, y="duration_cumsum", color=df_cumsum['is_music'])
st.plotly_chart(fig, use_container_width=True)

st.divider()

st.markdown('# Average daily duration')
df_1w = df[df['ses_start'] > pd.to_datetime('now') - pd.to_timedelta(7, unit='d')]
df_daily = df.groupby([df_1w['ses_start'].dt.date, 'is_music'])[['duration']].sum().reset_index()
fig = px.bar(df_daily, x=df_daily['ses_start'], y="duration", color=df_daily['is_music'])
st.plotly_chart(fig, use_container_width=True)

with st.sidebar:
    # Communication with ESP32
    st.markdown('# Command center')
    st.divider()
    
    display_text = ""
    
    st.markdown('## Dashboard')
    
    if st.button('Refresh data'):
        get_plays_data.clear()
        df = get_plays_data('playTable')
        display_text = '[SUPABASE] Data refreshed'
        print(display_text)
    
    st.divider()
    st.markdown('## ESP32 connection')
    
    if st.button('Init connection'):
        wst = threading.Thread(target=wapp.run_forever)
        wst.daemon = True
        wst.start()
        
        display_text = '[ESP] Connection initialized'
        print(display_text)

    if st.button('Close connection'):
        wapp.close()
        
        display_text = '[ESP] Connection closed'
        print(display_text)

    if st.button('Start FFT'):
        wapp.send('START')
        
        display_text = '[ESP] FFT started'
        print(display_text)

    if st.button('Stop FFT'):
        wapp.send('STOP')
        
        display_text = '[ESP] FFT stopped'
        print(display_text)
    
    st.divider()
    st.markdown('## Logs')
    
    if display_text != "":
        st.write('**Last log:** ' + display_text)

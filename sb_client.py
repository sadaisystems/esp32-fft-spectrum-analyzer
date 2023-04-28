import os
from supabase import create_client, Client
import streamlit as st


url: str = st.secrets["SUPABASE_URL"]
key: str = st.secrets["SUPABASE_KEY"]

supabase: Client = create_client(url, key)

response = supabase.table('playTable').select("*").execute()

print(response)

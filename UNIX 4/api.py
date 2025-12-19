import os
import json
import asyncio
from loguru import logger
from fastapi import FastAPI
from pydantic import BaseModel
from aiokafka import AIOKafkaProducer

KAFKA_BOOTSTRAP_SERVERS = os.getenv("KAFKA_BOOTSTRAP_SERVERS", "kafka:9092")
TASK_TOPIC = "tasks_topic"

app = FastAPI()
producer = None

class TextData(BaseModel):
    text: str

@app.on_event("startup")
async def startup_event():
    global producer
    retry_count = 0
    max_retries = 25
    
    while retry_count < max_retries:
        try:
            logger.info(f"Attempting to connect to Kafka ({retry_count + 1}/{max_retries})...")
            producer = AIOKafkaProducer(
                bootstrap_servers=KAFKA_BOOTSTRAP_SERVERS, 
                value_serializer=lambda v: json.dumps(v).encode('utf-8')
                )
            await producer.start()
            logger.info("Producer connected successfully!")
            return
        except Exception as e:
            logger.warning(f"Connection attempt failed: {e}")
            retry_count += 1
            await asyncio.sleep(5)
            
    raise Exception("Could not connect to Kafka after multiple retries")

@app.on_event("shutdown")
async def shutdown_event():
    if producer:
        await producer.stop()
        logger.info("Kafka producer stopped successfully.")

@app.post("/analyze", status_code=202)
async def analyze_text(data: TextData):    

    await producer.send_and_wait(
        TASK_TOPIC, 
        {"input_text": data.text,}
    )

    return {
        "status": "queued",
        "message": "Text received, analysis result will be provided by workers."
    }
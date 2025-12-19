import os
import json
import asyncio
import signal
from loguru import logger
import socket
from aiokafka import AIOKafkaConsumer

KAFKA_BOOTSTRAP_SERVERS = os.getenv("KAFKA_BOOTSTRAP_SERVERS", "kafka:9092")
TASK_TOPIC = "tasks_topic"

CONSUMER_GROUP_ID = "async_analyzer_workers_group" 

WORKER_ID = socket.gethostname()

shutdown_event = asyncio.Event()

def handle_sigterm():
    logger.warning("SIGTERM received — finishing current task before shutdown...")
    shutdown_event.set()

async def process_task(task: dict, worker_id: str):
    input_text = task.get("input_text", "")
    
    char_count = len(input_text)
    parity = "even" if char_count % 2 == 0 else "odd"  # чёт/нечёт

    await asyncio.sleep(2)  

    logger.info(f"[{worker_id}] Task processed.")
    logger.info(f"Source text: \"{input_text[:30]}...\"")
    logger.info(f"Length: {char_count} characters -> {parity.upper()}.\n")

async def run_worker():
    loop = asyncio.get_running_loop()

    loop.add_signal_handler(signal.SIGTERM, handle_sigterm)
    loop.add_signal_handler(signal.SIGINT, handle_sigterm)

    logger.info(f"Worker [{WORKER_ID}] starting up...")
    
    consumer = None
    retry_count = 0
    max_retries = 25

    while retry_count < max_retries:
        try:
            logger.info(f"Worker [{WORKER_ID}] connecting to Kafka ({retry_count + 1}/{max_retries})...")
            consumer = AIOKafkaConsumer(
                TASK_TOPIC,
                bootstrap_servers=KAFKA_BOOTSTRAP_SERVERS,
                group_id=CONSUMER_GROUP_ID,
                auto_offset_reset='latest',
                enable_auto_commit=False,
                value_deserializer=lambda x: json.loads(x.decode('utf-8'))
            )
            await consumer.start()
            logger.info(f"Worker [{WORKER_ID}] connected and listening to topic '{TASK_TOPIC}'...")
            break
        except Exception as e:
            logger.warning(f"Worker connection failed: {e}")
            retry_count += 1
            await asyncio.sleep(5)
    
    if not consumer:
        logger.critical(f"Critical: Worker [{WORKER_ID}] could not connect to Kafka after {max_retries} retries. Exiting.")
        return

    try:
        while not shutdown_event.is_set():
            msg = await consumer.getone()
            task = msg.value
            await process_task(task, WORKER_ID)

            await consumer.commit()
    finally:
        await consumer.stop()
        logger.info(f"Worker [{WORKER_ID}] stopped.")

if __name__ == "__main__":
    asyncio.run(run_worker())
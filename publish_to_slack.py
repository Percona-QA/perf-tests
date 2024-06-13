#!/usr/bin/python

# Create a script that publishes exactly 3 files (given as params) to Slack channel using webhooks
# Use pandas to convert input files from CSV to markdown
# webhook_url should be given as WEBHOOK_URL env param
# SLACK_MESSAGE env param will contatain an additional text message that will be included at the beginning of a slack message

import os
import sys
import requests
import pandas as pd

# Define constant headers for each file
HEADERS = ["QPS results:", "Difference in percentages to the average QPS:", "Standard deviation as a percentage of the average QPS:"]

def csv_to_markdown(file_path):
    df = pd.read_csv(file_path)
    df = df.applymap(lambda x: round(x) if isinstance(x, (int, float)) and x > 1000 else x)
    markdown = df.to_markdown(index=False, tablefmt='presto')
    return markdown

def send_to_slack(webhook_url, payload):
    response = requests.post(webhook_url, json=payload)
    if response.status_code != 200:
        print("Failed to publish to Slack. Status code: {}".format(response.status_code))
    else:
        print("Published successfully to Slack.")

def publish_to_slack(webhook_url, message, files):
    if message:
        payload = {
            "text": message
        }
        send_to_slack(webhook_url, payload)

    # Send each file as a separate message
    for file_path, header in zip(files, HEADERS):
        markdown_content = csv_to_markdown(file_path)
        code_block = "{}```{}```".format(header, markdown_content)
        payload = {
            "text": code_block
        }
        send_to_slack(webhook_url, payload)

if __name__ == "__main__":
    webhook_url = os.getenv("SLACK_WEBHOOK_URL")
    message = os.getenv("SLACK_MESSAGE")
    file_paths = sys.argv[1:4]

    if not webhook_url:
        print("SLACK_WEBHOOK_URL environment variable is not set.")
        exit(1)

    if not message:
        print("SLACK_MESSAGE environment variable is not set.")
        exit(1)

    if len(file_paths) != 3:
        print("Exactly 3 input files are required.")
        exit(1)

    publish_to_slack(webhook_url, message.replace("\\n", "\n"), file_paths)

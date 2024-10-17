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

def get_prefix_ending_at_line(content, max_length):
    # Return the longest prefix of content that ends at a line break and is smaller or equal to max_length.
    if len(content) == 0:
        return ""

    # Check if the entire content is within the limit
    if len(content) <= max_length:
        return content

    # Find the last newline character within the limit
    last_newline = content.rfind('\n', 0, max_length)

    if last_newline == -1:  # No newline found
        return content[:max_length]

    return content[:last_newline].strip()

def publish_to_slack(webhook_url, message, files):
    if message:
        payload = {
            "text": message
        }
        send_to_slack(webhook_url, payload)

    # Send each file as a separate message
    for file_path, header in zip(files, HEADERS):
        markdown_content = csv_to_markdown(file_path)
        code_block = "{}```{}".format(header, markdown_content)

        # Split the message using the get_prefix_ending_at_line function
        while code_block:
            chunk = get_prefix_ending_at_line(code_block, max_length=4000-3)
            if not chunk:
                break  # Break if there's nothing left to send
            payload = {
                "text": chunk + "```"
            }
            send_to_slack(webhook_url, payload)

            if len(code_block) == len(chunk):
                break  # Break if there's nothing left to send
            # Remove the sent chunk from code_block
            code_block = code_block[len(chunk):]
            code_block= "```" + code_block

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

#include "PrioWebcss.h"

String getStyling()
{
  String Style PROGMEM = R"(<style>
       body {
        margin: 0;
        font-family: Arial, sans-serif;
      }
      .prio-mr5 {
        margin-right: 5px;
      }
      .prio-ml5 {
        margin-left: 5px;
      }

      .prio-p2{
        padding: 2px;
      }
      /* top menu */
      .top-menu {
        background-color: #333;
        overflow: hidden;
      }
      .top-menu a {
        float: left;
        display: block;
        color: white;
        text-align: center;
        padding: 14px 16px;
        text-decoration: none;
      }
      .top-menu a:hover {
        background-color: #ddd;
        color: black;
      }

      .alarm-badge {
        float: right;
        margin: 10px 12px;
        padding: 4px 10px;
        border-radius: 12px;
        font-size: 12px;
        font-weight: bold;
        color: #fff;
      }

      .alarm-uit {
        background: #777;
      }

      .alarm-snooze {
        background: #d48a00;
      }

      .alarm-actief {
        background: #0a8f3c;
      }

      .alarm-ingesteld {
        background: #2c6fb7;
      }
      /* content */
      .content {
        margin-top: 10px;
        margin-left: 10px;
        font-family: Arial, sans-serif;
      }

      .stream_item {
        padding: 10px;
        border: 2px solid lightgray;
        border-radius: 8px;
        margin-bottom: 5px;
        max-width: 50vh;
        cursor: move; /* Add cursor style */
      }
      .content input {
        padding: 5px;
        font-family: Arial, sans-serif;
      }
      .input_long {
        border-radius: 5px;
        border-width: thin;
        width: 48vh;
      }

      .input_short {
        border-radius: 5px;
        border-width: thin;
      }
      .input_button {
        border-radius: 5px;
        border-width: thin;
        padding-left: 10px;
        padding-right: 10px;
        margin-left: 46vh;
        margin-top: 10px;
      }

      .url-container {
        margin-top: 15px;
        margin-bottom: 15px;
      }
      .edit-label {
        margin-left: 5px;
        font-size: 14px;
        font-weight: bold;
        margin-bottom: 2px;
      }

      .container {
        display: flex;
        align-items: center;
        margin-bottom: 10px;
        justify-content: space-between;
        width: 31vw;
      }
      .pagination {
        margin-right: 20px;
      }

      .pagesize-container {
        display: flex;
        align-items: center;
      }

      /* animated saver loader */
      #saving {
        position: absolute;
        top: 45px;
        left: 0px;
        width: 100%;
        height: 100%;
        background-color: rgba(255, 255, 255, 0.5);
        display: none;
        z-index: 999;
      }

      .loader {
        position: absolute;
        left: 50vw;
        border: 8px solid #f3f3f3;
        border-top: 8px solid #3498db;
        border-radius: 50%;
        width: 50px;
        height: 50px;
        animation: spin 1s linear infinite;
        top: 50vh;
      }

      @keyframes spin {
        0% {
          transform: rotate(0deg);
        }
        100% {
          transform: rotate(360deg);
        }
      }

      /* drag and Drop */
      .drop-zone {
        border: 2px dashed lightgray; /* Border style for drop zone */
        border-radius: 8px;
      }
      .drag-over {
        background-color: rgba(
          0,
          0,
          0,
          0.1
        ); /* Background color for drag-over */
      }
            .delete-icon-container {
        position: relative;
        width: 20px;
        height: 20px;
        cursor: pointer;
        float: inline-end;
        margin-top: -21px;
        margin-right: -4px;
      }

      .delete-icon {
        position: relative;
        width: 20px;
        height: 20px;
        cursor: pointer;
        float: inline-end;
        padding-left: 7px;
      }

      .delete-icon:before,
      .delete-icon:after {
        content: "";
        position: absolute;
        width: 2px;
        height: 16px;
        background-color: red;
        top: 2px;
        transform-origin: center;
        margin-left: 5px;
      }

      .delete-icon:before {
        transform: rotate(45deg);
      }

      .delete-icon:after {
        transform: rotate(-45deg);
      }

      .delete-icon-container:hover .delete-icon {
        background-color: rgba(255, 0, 0, 0.5); /* Oplichtend effect */
      }

      .delete-icon:hover::after,
      .delete-icon:hover:before {
        background-color: white;
      }

      #numberofStreams {
        margin-left: auto;
      }

      .alarm-header {
        display: flex;
        gap: 10px;
        align-items: center;
        margin-bottom: 12px;
      }

      .alarm-list {
        display: flex;
        flex-direction: column;
        gap: 10px;
        max-width: 900px;
      }

      .alarm-item {
        border: 1px solid #d1d1d1;
        border-radius: 8px;
        padding: 12px;
        background: #fafafa;
      }

      .alarm-grid {
        display: grid;
        grid-template-columns: repeat(auto-fit, minmax(130px, 1fr));
        gap: 10px;
        align-items: end;
      }

      .alarm-grid label {
        display: block;
        font-size: 12px;
        margin-bottom: 4px;
      }

      .alarm-grid input,
      .alarm-grid select {
        width: 100%;
        padding: 6px;
      }

      .alarm-days {
        display: flex;
        gap: 8px;
        flex-wrap: wrap;
        margin-top: 8px;
      }

      .alarm-day {
        display: flex;
        align-items: center;
        gap: 4px;
        font-size: 13px;
      }

      .alarm-actions {
        margin-top: 8px;
        display: flex;
        justify-content: flex-end;
      }

      /* File upload styling */
      .file-upload-container {
        margin-top: 10px;
        padding: 10px;
        border: 1px dashed #ccc;
        border-radius: 4px;
        background-color: #f9f9f9;
      }

      .file-upload-label {
        display: block;
        padding: 8px 12px;
        background-color: #4CAF50;
        color: white;
        border-radius: 4px;
        cursor: pointer;
        text-align: center;
        margin-bottom: 8px;
      }

      .file-upload-label:hover {
        background-color: #45a049;
      }

      #file_logo {
        display: none;
      }

      .file-info {
        font-size: 12px;
        color: #666;
        margin-top: 4px;
      }

      .logo-preview {
        max-width: 100px;
        max-height: 100px;
        margin-top: 8px;
        border: 1px solid #ddd;
        border-radius: 4px;
        display: none;
      }

      .logo-action-buttons {
        display: flex;
        gap: 8px;
        margin-top: 8px;
      }

      .refresh-logo-btn, .upload-logo-btn {
        padding: 6px 12px;
        color: white;
        border: none;
        border-radius: 4px;
        cursor: pointer;
        font-size: 13px;
        text-align: center;
      }

      .refresh-logo-btn {
        background-color: #2196F3;
        flex: 1;
      }

      .refresh-logo-btn:hover {
        background-color: #0b7dda;
      }
      
      .upload-logo-btn {
        background-color: #4CAF50;
        flex: 1;
        display: inline-block;
      }
      
      .upload-logo-btn:hover {
        background-color: #45a049;
      }
      
      .upload-logo-input {
        display: none;
      }

      .import-export-status {
        padding: 8px 10px;
        border-radius: 6px;
        max-width: 700px;
        font-size: 14px;
      }

      .import-export-status.ok {
        background: #e8f6ec;
        border: 1px solid #8ec9a0;
        color: #165a2e;
      }

      .import-export-status.error {
        background: #fdebec;
        border: 1px solid #e39b9f;
        color: #8d1f26;
      }
    </style>
    )";
  return Style;
}

#include "PrioWebcss.h"


String getStyling()
{
  String Style PROGMEM = R"(<style>
      body {
        margin: 0;
        font-family: Arial, sans-serif;
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
        max-width: 52vh;
        cursor: move; /* Add cursor style */
      }
      .content input {
        padding: 5px;
        font-family: Arial, sans-serif;
      }
      .input_url {
        border-radius: 5px;
        border-width: thin;
        width: 48vh;
      }

      .input_naam {
        border-radius: 5px;
        border-width: thin;
      }
      .input_button {
        border-radius: 5px;
        border-width: thin;
        padding-left: 10px;
        padding-right: 10px;
        margin-left: 45vh;
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
    </style>
    )";
  return Style;
}


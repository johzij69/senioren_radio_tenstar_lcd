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
    </style>
    )";
  return Style;
}

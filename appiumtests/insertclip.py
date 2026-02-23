#!/usr/bin/env python3

# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2016 Microsoft Corporation. All rights reserved.
# SPDX-FileCopyrightText: 2021-2022 Harald Sitter <sitter@kde.org>
# SPDX-FileCopyrightText: 2025 Jean-Baptiste Mardelle <jb@kdenlive.org>

import unittest
from appium import webdriver
from appium.webdriver.common.appiumby import AppiumBy
from appium.options.common.base import AppiumOptions
import selenium.common.exceptions
from selenium.webdriver.support.ui import WebDriverWait


class SimpleKdenliveTests(unittest.TestCase):

    @classmethod
    def setUpClass(self):
        options = AppiumOptions()
        # The app capability may be a command line or a desktop file id.
        options.set_capability("app", "org.kde.kdenlive.desktop")
        options.set_capability("args", "--no-welcome")

        # Boilerplate, always the same
        self.driver = webdriver.Remote(command_executor='http://127.0.0.1:4723', options=options)
        # Set a timeout for waiting to find elements. If elements cannot be found
        # in time we'll get a test failure. This should be somewhat long so as to
        # not fall over when the system is under load, but also not too long that
        # the test takes forever.
        self.driver.implicitly_wait = 10

    @classmethod
    def tearDownClass(self):
        # Make sure to terminate the driver again, lest it dangles.
        self.driver.quit()

    def setUp(self):
        wait = WebDriverWait(self.driver, 20)

        #wait.until(lambda x: self.getresults() == '0')

    def getStatusText(self):
        displaytext = self.driver.find_element(by=AppiumBy.ACCESSIBILITY_ID,
        value="QApplication.MainWindow#1.QStatusBar.StatusBarMessageLabel.FlashLabel.QLabel").text
        return displaytext

    def assertResult(self, actual, expected):
        wait = WebDriverWait(self.driver, 20)
        try:
            wait.until(lambda x: self.getStatusText() == expected)
        except selenium.common.exceptions.TimeoutException:
            pass
        self.assertEqual(self.getStatusText(), expected)

    def test_initialize(self):
        # Close welcome screen
        self.driver.find_element(by=AppiumBy.NAME, value="Start Editing").click()

        self.driver.find_element(by=AppiumBy.NAME, value="Add Color Clip…").click()
        self.driver.find_element(by=AppiumBy.ACCESSIBILITY_ID, value="QApplication.ColorClip_UI.buttonBox.QPushButton").click()
        # insert clip in timeline
        self.driver.find_element(by=AppiumBy.NAME, value="Insert Clip Zone in Timeline").click()
        # insert clip in timeline again
        self.driver.find_element(by=AppiumBy.NAME, value="Insert Clip Zone in Timeline").click()
        # select all in Bin
        self.driver.find_element(by=AppiumBy.NAME, value="Select All").click()
        #wait = WebDriverWait(self.driver, 2)
        self.assertResult(self.getStatusText(), "2 items selected (00:10:00) |")


if __name__ == '__main__':
    unittest.main()
